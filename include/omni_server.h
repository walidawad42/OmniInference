#pragma once

#include "omni_engine.h"
#include "tool_calling_interface.h"

#include "api/chat_request.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
using nlohmann::json;

// ============================================================
// OMNI SERVER - OpenAI- + Anthropic-compatible API server
// ============================================================
//
// Listens on a single TCP port and exposes:
//
//   GET  /health
//   GET  /v1/models
//   POST /v1/chat/completions    (OpenAI, streaming + non-streaming + tools)
//   POST /v1/completions         (OpenAI legacy)
//   POST /v1/embeddings          (501 unless an embedding model is loaded)
//   POST /v1/messages            (Anthropic, streaming + non-streaming + tools)
//   POST /v1/models/load
//   POST /tool/call
//   GET  /quantization/status
//
// All responses include CORS headers; OPTIONS preflight is handled
// automatically. When the `api_key` field of OmniServer::Config is set,
// every request needs an `Authorization: Bearer <key>` header.

struct OmniServerConfig {
    // TCP port to bind. Default 8080 matches the OpenAI tooling convention.
    int         port             = 8080;
    // If non-empty, every request must include `Authorization: Bearer <key>`.
    std::string api_key;
    // When true, all completions are routed through the deterministic mock
    // engine instead of OmniEngine. Lets the API surface be exercised on a
    // host with no model file / no GPU.
    bool        mock_mode        = false;
    // Origin allowed in CORS headers. "*" by default; set to a specific
    // origin if you want to lock the API down for browser clients.
    std::string cors_origin      = "*";
    // Logical model id reported by /v1/models when no real engine model is
    // loaded yet (handy for clients that filter on model id before sending).
    std::string default_model_id = "omni-inference-mock";
};

class HTTPServer;  // pImpl, defined in omni_server.cpp

class OmniServer {
public:
    explicit OmniServer(const OmniServerConfig& cfg = {});
    ~OmniServer();

    bool Start();
    bool Stop();
    bool IsRunning() const { return is_running_.load(); }

    int  Port() const { return cfg_.port; }
    bool MockMode() const { return cfg_.mock_mode; }

    OmniEngine&            Engine() { return *engine_; }
    ToolCallingInterface&  Tools()  { return *tool_calling_; }

    // Wired up internally by Start(). Public so tests can introspect the
    // route table without needing a live socket.
    void SetupRoutes();

private:
    OmniServerConfig                       cfg_;
    std::atomic<bool>                      is_running_{false};
    std::unique_ptr<HTTPServer>            http_server_;
    std::thread                            server_thread_;
    std::unique_ptr<OmniEngine>            engine_;
    std::unique_ptr<ToolCallingInterface>  tool_calling_;

    // ====== Non-streaming JSON handlers ======
    json HandleHealthCheck();
    json HandleModels();
    json HandleLoadModel(const json& request);
    json HandleEmbeddings(const json& request);
    json HandleToolCall(const json& request);
    json HandleQuantizationStatus();

    // ====== Streaming-aware handlers ======
    // Returns true if the response was streamed; otherwise the caller serves
    // the populated `out_response` as a regular non-streaming JSON body.
    using StreamSink = std::function<bool(const char* data, size_t len)>;
    bool HandleOpenAIChatCompletion(const json& request, json& out_response, StreamSink sink);
    bool HandleOpenAICompletion(const json& request, json& out_response, StreamSink sink);
    bool HandleAnthropicMessages(const json& request, json& out_response, StreamSink sink);

    // Drives either the real OmniEngine or MockEngine depending on cfg_.mock_mode.
    void RunChat(const omni::api::ChatRequest& req,
                 omni::api::ChatResponse& out_response);
    void StreamChat(const omni::api::ChatRequest& req,
                    const omni::api::StreamCallback& cb);
};
