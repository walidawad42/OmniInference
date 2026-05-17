#pragma once

#include "omni_engine.h"
#include "tool_calling_interface.h"
#include <string>
#include <memory>
#include <thread>
#include <json.hpp>

using json = nlohmann::json;

// ============================================================
// OMNI SERVER - OpenAI-Compatible API Server
// REST API for model inference, tool calling, and more
// ============================================================

class OmniServer {
public:
    OmniServer(int port = 8000);
    ~OmniServer();

    bool Start();
    bool Stop();
    bool IsRunning() const { return is_running_; }

    // API Endpoints
    void SetupRoutes();

private:
    int port_ = 8000;
    bool is_running_ = false;
    std::unique_ptr<class HTTPServer> http_server_;
    std::thread server_thread_;
    std::unique_ptr<OmniEngine> engine_;
    std::unique_ptr<ToolCallingInterface> tool_calling_;

    // Route handlers
    json HandleHealthCheck();
    json HandleModels();
    json HandleLoadModel(const json& request);
    json HandleChatCompletion(const json& request);
    json HandleCompletion(const json& request);
    json HandleEmbeddings(const json& request);
    json HandleToolCall(const json& request);
    json HandleQuantizationStatus();
};

// ============================================================
// HTTP SERVER IMPLEMENTATION
// ============================================================

class HTTPServer {
public:
    HTTPServer(int port);
    ~HTTPServer();

    bool Start();
    bool Stop();

    using RouteHandler = std::function<json(const std::string&)>;
    void RegisterRoute(const std::string& method, const std::string& path, RouteHandler handler);

private:
    int port_ = 0;
    bool running_ = false;
    void* server_handle_ = nullptr;

    static void HandleRequest(const std::string& method, const std::string& path, const std::string& body);
};