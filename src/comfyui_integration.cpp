#include "comfyui_integration.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
    #define PLATFORM_WINDOWS 1
#else
    #include <unistd.h>
    #define PLATFORM_LINUX 1
#endif

ComfyUIIntegration::ComfyUIIntegration(const ComfyUIConfig& config) : config_(config) {
    curl_handle_ = curl_easy_init();
}

ComfyUIIntegration::~ComfyUIIntegration() {
    StopServer();
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

bool ComfyUIIntegration::StartServer() {
    if (IsServerRunning()) {
        std::cout << "[ComfyUI] Server already running" << std::endl;
        return true;
    }

    std::string command;

#ifdef PLATFORM_WINDOWS
    command = "start /B python -m comfyui.server --port " + std::to_string(config_.port) + " --gpu-device " + std::to_string(config_.device_index);
#else
    command = "python -m comfyui.server --port " + std::to_string(config_.port) + " --gpu-device " + std::to_string(config_.device_index) + " &";
#endif

    std::cout << "[ComfyUI] Starting server: " << command << std::endl;

    int result = system(command.c_str());
    if (result == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return ConnectToServer();
    }

    return false;
}

bool ComfyUIIntegration::StopServer() {
    if (!IsServerRunning()) {
        return true;
    }

#ifdef PLATFORM_WINDOWS
    system("taskkill /F /IM comfyui.exe");
#else
    system("pkill -f 'comfyui.server'");
#endif

    return true;
}

bool ComfyUIIntegration::IsServerRunning() {
    try {
        CURL* test_curl = curl_easy_init();
        curl_easy_setopt(test_curl, CURLOPT_URL, (config_.server_url + "/system_stats").c_str());
        curl_easy_setopt(test_curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(test_curl, CURLOPT_CONNECTTIMEOUT, 2L);

        CURLcode res = curl_easy_perform(test_curl);
        curl_easy_cleanup(test_curl);

        return res == CURLE_OK;
    } catch (...) {
        return false;
    }
}

bool ComfyUIIntegration::ConnectToServer() {
    if (IsServerRunning()) {
        is_connected_ = true;
        std::cout << "[ComfyUI] Connected to server at " << config_.server_url << std::endl;
        return true;
    }

    if (config_.auto_start_server) {
        return StartServer() && ConnectToServer();
    }

    return false;
}

std::vector<std::string> ComfyUIIntegration::GetAvailableModels() {
    std::vector<std::string> models;

    std::string response = MakeRequest("/models", "GET");

    try {
        json data = json::parse(response);

        if (data.contains("checkpoints")) {
            for (const auto& model : data["checkpoints"]) {
                models.push_back(model.get<std::string>());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ComfyUI] Parse error: " << e.what() << std::endl;
    }

    return models;
}

bool ComfyUIIntegration::LoadModel(const std::string& model_name) {
    json payload{};
    payload["model_name"] = model_name;

    std::string response = MakeRequest("/load_model", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("success", false);
    } catch (...) {
        return false;
    }
}

std::string ComfyUIIntegration::GenerateImage(
    const std::string& prompt,
    int width,
    int height,
    int steps,
    float guidance_scale) {

    json workflow = json::object();

    // Build basic stable diffusion workflow
    workflow["1"] = {
        {"class_type", "CheckpointLoaderSimple"},
        {"inputs", {{"ckpt_name", "model.safetensors"}}}
    };

    workflow["2"] = {
        {"class_type", "CLIPTextEncode"},
        {"inputs", {{"text", prompt}, {"clip", json::array({1, 0})}}}
    };

    workflow["3"] = {
        {"class_type", "KSampler"},
        {"inputs", {
            {"seed", 12345},
            {"steps", steps},
            {"cfg", guidance_scale},
            {"sampler_name", "euler"},
            {"scheduler", "normal"},
            {"denoise", 1.0},
            {"model", json::array({1, 0})},
            {"positive", json::array({2, 0})},
            {"negative", json::array({2, 0})},
            {"latent_image", json::array({4, 0})}
        }}
    };

    workflow["4"] = {
        {"class_type", "EmptyLatentImage"},
        {"inputs", {{"width", width}, {"height", height}, {"batch_size", 1}}}
    };

    workflow["5"] = {
        {"class_type", "VAEDecode"},
        {"inputs", {{"samples", json::array({3, 0})}, {"vae", json::array({1, 0})}}}
    };

    workflow["6"] = {
        {"class_type", "SaveImage"},
        {"inputs", {{"images", json::array({5, 0})}, {"filename_prefix", "omni_"}}}
    };

    std::string task_id = SubmitTask(ComfyUIWorkflow{"", "", workflow, {}, ""}, json());

    return task_id;
}

std::string ComfyUIIntegration::SubmitTask(const ComfyUIWorkflow& workflow, const json& inputs) {
    json payload = workflow.workflow_json;
    payload["inputs"] = inputs;

    std::string response = MakeRequest("/prompt", "POST", payload);

    try {
        json data = json::parse(response);
        return data.value("prompt_id", "");
    } catch (const std::exception& e) {
        std::cerr << "[ComfyUI] Submit error: " << e.what() << std::endl;
        return "";
    }
}

ComfyUITaskStatus ComfyUIIntegration::GetTaskStatus(const std::string& task_id) {
    std::string response = MakeRequest("/history/" + task_id, "GET");

    ComfyUITaskStatus status{};
    status.task_id = task_id;
    status.status = "unknown";

    try {
        json data = json::parse(response);

        if (data.contains(task_id)) {
            auto task = data[task_id];
            if (task.contains("status")) {
                status.status = task["status"].value("status", "unknown");
                status.progress = task["status"].value("value", 0.0f) / task["status"].value("max", 1.0f);
            }
            if (task.contains("outputs")) {
                status.result = task["outputs"];
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ComfyUI] Status error: " << e.what() << std::endl;
        status.error_message = e.what();
    }

    return status;
}

ComfyUITaskStatus ComfyUIIntegration::PollTask(const std::string& task_id, int timeout_seconds) {
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed > timeout_seconds) {
            ComfyUITaskStatus status{};
            status.task_id = task_id;
            status.status = "timeout";
            return status;
        }

        ComfyUITaskStatus status = GetTaskStatus(task_id);

        if (status.status == "completed" || status.status == "error") {
            return status;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

std::string ComfyUIIntegration::MakeRequest(const std::string& endpoint, const std::string& method, const json& data) {
    std::string url = config_.server_url + endpoint;

    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, method.c_str());

    std::string response;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    if (method != "GET" && !data.is_null()) {
        std::string json_data = data.dump();
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, json_data.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl_handle_);

    if (res != CURLE_OK) {
        std::cerr << "[ComfyUI] Request error: " << curl_easy_strerror(res) << std::endl;
    }

    return response;
}