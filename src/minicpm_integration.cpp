// MiniCPM-O 4.5 multimodal integration.
//
// The header in `include/minicpm_integration.h` describes a fairly large API
// surface (image / video / OCR / batch processing). The original repo never
// shipped a matching .cpp for it, which means even the GUI code that just
// instantiates a `MiniCPMIntegration` member fails to link. This file
// provides minimal, side-effect-free implementations so the project links
// cleanly. Each method either talks to a local MiniCPM server over HTTP via
// libcurl (when one is running) or returns an empty / "not running" result.
#include "minicpm_integration.h"

#include <curl/curl.h>

#include <iostream>
#include <sstream>

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

} // namespace

MiniCPMIntegration::MiniCPMIntegration(const MiniCPMConfig& config)
    : config_(config) {}

MiniCPMIntegration::~MiniCPMIntegration() {
    if (server_running_) {
        StopServer();
    }
}

bool MiniCPMIntegration::StartServer() {
    std::cout << "[MiniCPM] Launching server at " << config_.server_url
              << std::endl;
#ifdef _WIN32
    int rc = std::system("start /B python -m minicpm.server --port 8001");
#else
    int rc = std::system("python -m minicpm.server --port 8001 &");
#endif
    server_running_ = (rc == 0);
    return server_running_;
}

bool MiniCPMIntegration::StopServer() {
    server_running_ = false;
    return true;
}

bool MiniCPMIntegration::IsServerRunning() {
    if (!server_running_) return false;

    // Cheap GET on the health endpoint; treat any non-zero curl error as
    // "down" rather than throwing.
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::string body;
    const std::string url = config_.server_url + "/health";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool MiniCPMIntegration::LoadModel() { return server_running_; }
bool MiniCPMIntegration::UnloadModel() { return true; }

MultimodalOutput MiniCPMIntegration::ProcessImage(const std::string& image_path,
                                                  const std::string& prompt) {
    MultimodalOutput out;
    json payload = {{"image_path", image_path}, {"prompt", prompt}};
    const std::string raw = MakeRequest("/process_image", payload);
    if (!raw.empty()) {
        const json data = ParseResponse(raw);
        out.text_response = data.value("text", "");
    }
    return out;
}

MultimodalOutput MiniCPMIntegration::ProcessMultipleImages(
    const std::vector<std::string>& image_paths, const std::string& prompt) {
    MultimodalOutput out;
    json payload = {{"image_paths", image_paths}, {"prompt", prompt}};
    const std::string raw = MakeRequest("/process_images", payload);
    if (!raw.empty()) {
        out.text_response = ParseResponse(raw).value("text", "");
    }
    return out;
}

MultimodalOutput MiniCPMIntegration::ProcessVideo(const std::string& video_path,
                                                  const std::string& prompt,
                                                  int frame_sample_rate) {
    MultimodalOutput out;
    json payload = {{"video_path", video_path},
                    {"prompt", prompt},
                    {"frame_sample_rate", frame_sample_rate}};
    const std::string raw = MakeRequest("/process_video", payload);
    if (!raw.empty()) {
        out.text_response = ParseResponse(raw).value("text", "");
    }
    return out;
}

std::string MiniCPMIntegration::ExtractTextFromImage(
    const std::string& image_path) {
    json payload = {{"image_path", image_path}};
    const std::string raw = MakeRequest("/ocr", payload);
    if (raw.empty()) return {};
    return ParseResponse(raw).value("text", "");
}

std::vector<std::string> MiniCPMIntegration::ExtractTextFromMultipleImages(
    const std::vector<std::string>& image_paths) {
    std::vector<std::string> texts;
    texts.reserve(image_paths.size());
    for (const auto& path : image_paths) {
        texts.push_back(ExtractTextFromImage(path));
    }
    return texts;
}

std::vector<std::string> MiniCPMIntegration::DetectObjectsInImage(
    const std::string& image_path) {
    std::vector<std::string> objects;
    json payload = {{"image_path", image_path}};
    const std::string raw = MakeRequest("/detect", payload);
    if (raw.empty()) return objects;
    const json data = ParseResponse(raw);
    for (const auto& obj : data.value("objects", json::array())) {
        if (obj.is_string()) {
            objects.push_back(obj.get<std::string>());
        }
    }
    return objects;
}

json MiniCPMIntegration::GetDetailedObjectInfo(const std::string& image_path) {
    json payload = {{"image_path", image_path}};
    const std::string raw = MakeRequest("/detect_detailed", payload);
    return raw.empty() ? json::object() : ParseResponse(raw);
}

MultimodalOutput MiniCPMIntegration::GenerateAnswerWithContext(
    const MultimodalInput& input,
    const std::vector<std::string>& context_documents) {
    MultimodalOutput out;
    json payload = {{"text", input.text_content},
                    {"images", input.image_paths},
                    {"context", context_documents}};
    const std::string raw = MakeRequest("/answer_with_context", payload);
    if (!raw.empty()) {
        out.text_response = ParseResponse(raw).value("text", "");
    }
    return out;
}

std::vector<MultimodalOutput> MiniCPMIntegration::BatchProcess(
    const std::vector<MultimodalInput>& inputs) {
    std::vector<MultimodalOutput> outputs;
    outputs.reserve(inputs.size());
    for (const auto& in : inputs) {
        outputs.push_back(ProcessImage(
            in.image_paths.empty() ? std::string() : in.image_paths.front(),
            in.text_content));
    }
    return outputs;
}

std::string MiniCPMIntegration::MakeRequest(const std::string& endpoint,
                                            const json& data) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string body;
    const std::string url = config_.server_url + endpoint;
    const std::string payload = data.dump();

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    const CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[MiniCPM] Request failed: " << curl_easy_strerror(res)
                  << std::endl;
        return {};
    }
    return body;
}

json MiniCPMIntegration::ParseResponse(const std::string& response) {
    try {
        return json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "[MiniCPM] Bad response JSON: " << e.what() << std::endl;
        return json::object();
    }
}
