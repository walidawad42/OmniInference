#include "dify_integration.h"
#include <iostream>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

DifyIntegration::DifyIntegration(const DifyConfig& config) : config_(config) {
    curl_handle_ = curl_easy_init();
}

DifyIntegration::~DifyIntegration() {
    StopServer();
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

bool DifyIntegration::StartServer() {
    std::string command;

#ifdef _WIN32
    command = "start /B python -m dify.main --port " + std::to_string(config_.port);
#else
    command = "python -m dify.main --port " + std::to_string(config_.port) + " &";
#endif

    std::cout << "[Dify] Starting server..." << std::endl;
    int result = system(command.c_str());

    if (result == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return ConnectToServer();
    }

    return false;
}

bool DifyIntegration::ConnectToServer() {
    if (IsServerRunning()) {
        is_connected_ = true;
        std::cout << "[Dify] Connected to server at " << config_.server_url << std::endl;
        return true;
    }

    if (config_.auto_start_server) {
        return StartServer();
    }

    return false;
}

std::vector<DifyApp> DifyIntegration::GetAvailableApps() {
    std::vector<DifyApp> apps;

    std::string response = MakeRequest(config_.server_url, "/apps", "GET");

    try {
        json data = json::parse(response);
        if (data.contains("data")) {
            for (const auto& app_data : data["data"]) {
                DifyApp app{};
                app.app_id = app_data.value("id", "");
                app.app_name = app_data.value("name", "");
                app.app_type = app_data.value("type", "");
                app.description = app_data.value("description", "");
                apps.push_back(app);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Dify] Parse error: " << e.what() << std::endl;
    }

    return apps;
}

DifyResponse DifyIntegration::Chat(
    const DifyApp& app,
    const std::string& message,
    const std::string& conversation_id,
    const json& variables) {

    json payload{};
    payload["query"] = message;
    payload["conversation_id"] = conversation_id;
    payload["variables"] = variables;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::string response = MakeRequest(config_.server_url, "/chat/messages", "POST", payload);

    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    DifyResponse dify_response{};
    dify_response.latency_ms = latency;

    try {
        json data = json::parse(response);
        dify_response.message_id = data.value("id", "");
        dify_response.content = data.value("answer", "");
        dify_response.metadata = data.value("metadata", json());

        if (data.contains("tool_calls")) {
            for (const auto& tool : data["tool_calls"]) {
                dify_response.tool_calls.push_back(tool.get<std::string>());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Dify] Response error: " << e.what() << std::endl;
    }

    return dify_response;
}

DifyResponse DifyIntegration::ExecuteWorkflow(const DifyApp& app, const json& workflow_inputs) {
    json payload = workflow_inputs;

    std::string response = MakeRequest(config_.server_url, "/workflows/run", "POST", payload);

    DifyResponse result{};

    try {
        json data = json::parse(response);
        result.content = data.dump();
    } catch (const std::exception& e) {
        std::cerr << "[Dify] Workflow error: " << e.what() << std::endl;
    }

    return result;
}

std::string DifyIntegration::MakeRequest(const std::string& server_url, const std::string& endpoint, const std::string& method, const json& data) {
    std::string url = server_url + endpoint;

    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, method.c_str());

    if (!config_.api_key.empty()) {
        std::string auth_header = "Authorization: Bearer " + config_.api_key;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    }

    std::string response;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    if (method != "GET" && !data.is_null()) {
        std::string json_data = data.dump();
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, json_data.c_str());
    }

    CURLcode res = curl_easy_perform(curl_handle_);

    if (res != CURLE_OK) {
        std::cerr << "[Dify] Request error: " << curl_easy_strerror(res) << std::endl;
    }

    return response;
}

bool DifyIntegration::IsServerRunning() {
    try {
        CURL* test_curl = curl_easy_init();
        curl_easy_setopt(test_curl, CURLOPT_URL, (config_.server_url + "/health").c_str());
        curl_easy_setopt(test_curl, CURLOPT_TIMEOUT, 2L);

        CURLcode res = curl_easy_perform(test_curl);
        curl_easy_cleanup(test_curl);

        return res == CURLE_OK;
    } catch (...) {
        return false;
    }
}

bool DifyIntegration::StopServer() {
#ifdef _WIN32
    system("taskkill /F /IM dify.exe");
#else
    system("pkill -f 'dify.main'");
#endif
    return true;
}