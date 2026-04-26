#include "omni_server.h"
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

// Simple HTTP request parser
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

OmniServer::OmniServer(int port) : port_(port) {
    engine_ = std::make_unique<OmniEngine>();
    tool_calling_ = std::make_unique<ToolCallingInterface>();
    http_server_ = std::make_unique<HTTPServer>(port);
}

OmniServer::~OmniServer() {
    Stop();
}

bool OmniServer::Start() {
    std::cout << "[OmniServer] Starting API server on port " << port_ << std::endl;

    // Initialize engine
    if (!engine_->InitializeBackend()) {
        std::cerr << "[OmniServer] Failed to initialize backend" << std::endl;
        return false;
    }

    engine_->PrintHardwareReport();

    // Setup routes
    SetupRoutes();

    // Start HTTP server
    if (!http_server_->Start()) {
        std::cerr << "[OmniServer] Failed to start HTTP server" << std::endl;
        return false;
    }

    is_running_ = true;

    std::cout << "[OmniServer] ✓ API Server started successfully" << std::endl;
    std::cout << "[OmniServer] OpenAI-compatible endpoints available at:" << std::endl;
    std::cout << "[OmniServer]   - POST /v1/chat/completions" << std::endl;
    std::cout << "[OmniServer]   - POST /v1/completions" << std::endl;
    std::cout << "[OmniServer]   - GET  /v1/models" << std::endl;
    std::cout << "[OmniServer]   - POST /v1/embeddings" << std::endl;
    std::cout << "[OmniServer]   - POST /tool/call" << std::endl;

    return true;
}

bool OmniServer::Stop() {
    if (!is_running_) return true;

    std::cout << "[OmniServer] Stopping API server..." << std::endl;

    if (http_server_) {
        http_server_->Stop();
    }

    is_running_ = false;

    std::cout << "[OmniServer] API server stopped" << std::endl;
    return true;
}

void OmniServer::SetupRoutes() {
    // Health check
    http_server_->RegisterRoute("GET", "/health", [this](const std::string& body) {
        return HandleHealthCheck();
    });

    // Models list
    http_server_->RegisterRoute("GET", "/v1/models", [this](const std::string& body) {
        return HandleModels();
    });

    // Load model
    http_server_->RegisterRoute("POST", "/v1/models/load", [this](const std::string& body) {
        try {
            json request = json::parse(body);
            return HandleLoadModel(request);
        } catch (...) {
            return json{{"error", "Invalid JSON"}};
        }
    });

    // Chat completion (OpenAI-compatible)
    http_server_->RegisterRoute("POST", "/v1/chat/completions", [this](const std::string& body) {
        try {
            json request = json::parse(body);
            return HandleChatCompletion(request);
        } catch (...) {
            return json{{"error", "Invalid JSON"}};
        }
    });

    // Text completion
    http_server_->RegisterRoute("POST", "/v1/completions", [this](const std::string& body) {
        try {
            json request = json::parse(body);
            return HandleCompletion(request);
        } catch (...) {
            return json{{"error", "Invalid JSON"}};
        }
    });

    // Embeddings
    http_server_->RegisterRoute("POST", "/v1/embeddings", [this](const std::string& body) {
        try {
            json request = json::parse(body);
            return HandleEmbeddings(request);
        } catch (...) {
            return json{{"error", "Invalid JSON"}};
        }
    });

    // Tool calling
    http_server_->RegisterRoute("POST", "/tool/call", [this](const std::string& body) {
        try {
            json request = json::parse(body);
            return HandleToolCall(request);
        } catch (...) {
            return json{{"error", "Invalid JSON"}};
        }
    });

    // Quantization status
    http_server_->RegisterRoute("GET", "/quantization/status", [this](const std::string& body) {
        return HandleQuantizationStatus();
    });
}

json OmniServer::HandleHealthCheck() {
    json response;
    response["status"] = "ok";
    response["version"] = "2.0.0";
    response["gpu_available"] = engine_->GetActiveBackend() != HardwareBackend::CPU_FALLBACK;
    response["timestamp"] = std::time(nullptr);

    return response;
}

json OmniServer::HandleModels() {
    json response;
    response["object"] = "list";

    json data = json::array();

    json model;
    model["id"] = "omni-inference-2.0";
    model["object"] = "model";
    model["created"] = std::time(nullptr);
    model["owned_by"] = "OmniInference";
    model["permission"] = json::array();
    model["root"] = "omni-inference-2.0";
    model["parent"] = nullptr;

    data.push_back(model);
    response["data"] = data;

    return response;
}

json OmniServer::HandleLoadModel(const json& request) {
    std::string model_path = request.value("model_path", "");

    if (model_path.empty()) {
        return json{{"error", "model_path required"}};
    }

    ModelParameters params;
    params.model_path = model_path;
    params.model_name = request.value("model_name", "default");
    params.n_ctx = request.value("n_ctx", 4096);
    params.n_batch = request.value("n_batch", 512);
    params.n_gpu_layers = request.value("n_gpu_layers", -1);

    TurboQuantConfig quant_cfg;
    quant_cfg.mode = QuantizationMode::TURBO_QUANT_3BIT;
    quant_cfg.key_bits = request.value("key_bits", 4);
    quant_cfg.value_bits = request.value("value_bits", 2);

    if (engine_->LoadModel(params, quant_cfg)) {
        json response;
        response["status"] = "success";
        response["model"] = params.model_name;
        response["loaded"] = true;
        return response;
    } else {
        return json{{"error", "Failed to load model"}};
    }
}

json OmniServer::HandleChatCompletion(const json& request) {
    std::string prompt = "";

    // Extract messages
    if (request.contains("messages")) {
        for (const auto& msg : request["messages"]) {
            std::string role = msg.value("role", "");
            std::string content = msg.value("content", "");

            if (role == "user") {
                prompt += content + "\n";
            }
        }
    }

    GenerationConfig gen_cfg;
    gen_cfg.temperature = request.value("temperature", 0.7f);
    gen_cfg.top_p = request.value("top_p", 0.95f);
    gen_cfg.max_tokens = request.value("max_tokens", 512);

    std::string response_text = engine_->Generate(prompt, gen_cfg);

    json response;
    response["id"] = "omni-" + std::to_string(std::time(nullptr));
    response["object"] = "chat.completion";
    response["created"] = std::time(nullptr);
    response["model"] = "omni-inference-2.0";

    json choice;
    choice["index"] = 0;
    choice["message"]["role"] = "assistant";
    choice["message"]["content"] = response_text;
    choice["finish_reason"] = "stop";

    response["choices"] = json::array({choice});

    json usage;
    usage["prompt_tokens"] = prompt.length() / 4;  // Rough estimate
    usage["completion_tokens"] = response_text.length() / 4;
    usage["total_tokens"] = (prompt.length() + response_text.length()) / 4;

    response["usage"] = usage;

    return response;
}

json OmniServer::HandleCompletion(const json& request) {
    std::string prompt = request.value("prompt", "");

    GenerationConfig gen_cfg;
    gen_cfg.temperature = request.value("temperature", 0.7f);
    gen_cfg.top_p = request.value("top_p", 0.95f);
    gen_cfg.max_tokens = request.value("max_tokens", 512);

    std::string response_text = engine_->Generate(prompt, gen_cfg);

    json response;
    response["id"] = "omni-" + std::to_string(std::time(nullptr));
    response["object"] = "text_completion";
    response["created"] = std::time(nullptr);
    response["model"] = "omni-inference-2.0";

    json choice;
    choice["text"] = response_text;
    choice["index"] = 0;
    choice["finish_reason"] = "stop";

    response["choices"] = json::array({choice});

    return response;
}

json OmniServer::HandleEmbeddings(const json& request) {
    std::string input = request.value("input", "");

    // Simple embedding mock (64-dim vector)
    std::vector<float> embedding(64);
    for (int i = 0; i < 64; i++) {
        embedding[i] = (i % 2 == 0) ? 0.1f : -0.1f;
    }

    json response;
    response["object"] = "list";

    json data;
    data["object"] = "embedding";
    data["index"] = 0;
    data["embedding"] = embedding;

    response["data"] = json::array({data});

    json usage;
    usage["prompt_tokens"] = input.length() / 4;
    usage["total_tokens"] = input.length() / 4;

    response["usage"] = usage;

    return response;
}

json OmniServer::HandleToolCall(const json& request) {
    std::string tool_name = request.value("tool_name", "");
    json arguments = request.value("arguments", json());

    if (tool_name.empty()) {
        return json{{"error", "tool_name required"}};
    }

    FunctionCall call;
    call.tool_name = tool_name;
    call.arguments = arguments;

    auto result = tool_calling_->ExecuteTool(call);

    json response;
    response["call_id"] = call.call_id;
    response["tool_name"] = tool_name;
    response["result"] = result.result;
    response["success"] = result.success;

    return response;
}

json OmniServer::HandleQuantizationStatus() {
    auto quant_cfg = engine_->GetCurrentQuantConfig();

    json response;
    response["mode"] = static_cast<int>(quant_cfg.mode);
    response["key_bits"] = quant_cfg.key_bits;
    response["value_bits"] = quant_cfg.value_bits;
    response["use_polar_transform"] = quant_cfg.use_polar_transform;
    response["use_qjl_correction"] = quant_cfg.use_qjl_correction;
    response["active"] = engine_->IsModelLoaded();

    return response;
}

HTTPServer::HTTPServer(int port) : port_(port) {}

HTTPServer::~HTTPServer() {
    Stop();
}

bool HTTPServer::Start() {
    std::cout << "[HTTPServer] Starting on port " << port_ << std::endl;
    // HTTP server implementation would go here
    // Using a library like cpp-httplib or asio
    running_ = true;
    return true;
}

bool HTTPServer::Stop() {
    running_ = false;
    return true;
}

void HTTPServer::RegisterRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    std::cout << "[HTTPServer] Registered route: " << method << " " << path << std::endl;
}