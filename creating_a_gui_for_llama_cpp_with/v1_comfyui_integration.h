#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

// ============================================================
// COMFYUI LOCAL INTEGRATION
// ============================================================

struct ComfyUIConfig {
    std::string server_url = "http://localhost:8188";
    int port = 8188;
    bool use_gpu = true;
    int device_index = 0;
    bool auto_start_server = true;
};

struct ComfyUIWorkflow {
    std::string workflow_id;
    std::string workflow_name;
    json workflow_json;
    std::vector<std::string> required_models;
    std::string description;
};

struct ComfyUITaskStatus {
    std::string task_id;
    float progress = 0.0f;
    std::string status; // "queued", "executing", "completed", "error"
    json result;
    std::string error_message;
};

class ComfyUIIntegration {
public:
    ComfyUIIntegration(const ComfyUIConfig& config = ComfyUIConfig());
    ~ComfyUIIntegration();

    // Server Management
    bool StartServer();
    bool StopServer();
    bool IsServerRunning();
    bool ConnectToServer();

    // Model Management
    std::vector<std::string> GetAvailableModels();
    bool LoadModel(const std::string& model_name);
    bool UnloadModel(const std::string& model_name);

    // Workflow Management
    bool LoadWorkflow(const std::string& workflow_path);
    bool SaveWorkflow(const std::string& workflow_name, const ComfyUIWorkflow& workflow);
    std::vector<ComfyUIWorkflow> GetAvailableWorkflows();

    // Task Execution
    std::string SubmitTask(const ComfyUIWorkflow& workflow, const json& inputs);
    ComfyUITaskStatus GetTaskStatus(const std::string& task_id);
    json GetTaskResult(const std::string& task_id);
    bool CancelTask(const std::string& task_id);

    // Image Generation
    std::string GenerateImage(
        const std::string& prompt,
        int width = 512,
        int height = 512,
        int steps = 20,
        float guidance_scale = 7.5f
    );

    // Video Generation
    std::string GenerateVideo(
        const std::string& prompt,
        int frames = 24,
        int fps = 8
    );

    // Upscaling
    std::string UpscaleImage(const std::string& image_path, int scale_factor = 4);

    // Polling
    ComfyUITaskStatus PollTask(const std::string& task_id, int timeout_seconds = 300);

private:
    ComfyUIConfig config_;
    CURL* curl_handle_ = nullptr;
    std::string server_process_id_;
    bool is_connected_ = false;

    std::string MakeRequest(const std::string& endpoint, const std::string& method, const json& data = json());
    json ParseResponse(const std::string& response);
};