#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
using nlohmann::json;

using json = nlohmann::json;

// ============================================================
// MINICPM-O 4.5 MULTIMODAL MODEL INTEGRATION
// ============================================================

struct MiniCPMConfig {
    std::string model_path;
    std::string server_url = "http://localhost:8001";
    bool use_gpu = true;
    int device_id = 0;
    float temperature = 0.7f;
    int max_tokens = 2048;
    bool use_quantization = true;
};

struct MultimodalInput {
    std::string text_content;
    std::vector<std::string> image_paths;
    std::vector<std::string> video_paths;
    std::string audio_path;
    json metadata;
};

struct MultimodalOutput {
    std::string text_response;
    std::vector<std::string> generated_image_paths;
    json structured_data;
    std::vector<std::string> detected_objects;
    std::vector<std::string> extracted_text;
};

class MiniCPMIntegration {
public:
    MiniCPMIntegration(const MiniCPMConfig& config = MiniCPMConfig());
    ~MiniCPMIntegration();

    // Server Management
    bool StartServer();
    bool StopServer();
    bool IsServerRunning();
    bool LoadModel();
    bool UnloadModel();

    // Text + Image Understanding
    MultimodalOutput ProcessImage(
        const std::string& image_path,
        const std::string& prompt
    );

    MultimodalOutput ProcessMultipleImages(
        const std::vector<std::string>& image_paths,
        const std::string& prompt
    );

    // Video Understanding
    MultimodalOutput ProcessVideo(
        const std::string& video_path,
        const std::string& prompt,
        int frame_sample_rate = 1
    );

    // OCR Capabilities
    std::string ExtractTextFromImage(const std::string& image_path);
    std::vector<std::string> ExtractTextFromMultipleImages(const std::vector<std::string>& image_paths);

    // Object Detection
    std::vector<std::string> DetectObjectsInImage(const std::string& image_path);
    json GetDetailedObjectInfo(const std::string& image_path);

    // RAG Context Integration
    MultimodalOutput GenerateAnswerWithContext(
        const MultimodalInput& input,
        const std::vector<std::string>& context_documents
    );

    // Batch Processing
    std::vector<MultimodalOutput> BatchProcess(const std::vector<MultimodalInput>& inputs);

private:
    MiniCPMConfig config_;
    bool server_running_ = false;
    std::string server_process_id_;

    std::string MakeRequest(const std::string& endpoint, const json& data);
    json ParseResponse(const std::string& response);
};