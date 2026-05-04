#include "omni_server.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

// cpp-httplib is header-only; the include lives in vendored sources.
// CPPHTTPLIB_THREAD_POOL_COUNT defaults to 8 which is fine for our use.
// Do NOT define CPPHTTPLIB_OPENSSL_SUPPORT - cpp-httplib uses #ifdef on it,
// so any value (including 0) compiles in the SSL pieces and demands the
// OpenSSL libraries at link time. Leaving it undefined disables SSL entirely
// and keeps OmniServer link-clean against the system libstdc++/pthread only.
#include <httplib.h>

#include "api/anthropic_schema.h"
#include "api/chat_request.h"
#include "api/image_decode.h"
#include "api/mock_engine.h"
#include "api/openai_schema.h"
#include "api/sse_writer.h"
#include "api/tool_translator.h"

namespace api = omni::api;

// ============================================================
// HTTPServer pImpl
// ============================================================
class HTTPServer {
public:
    httplib::Server server;
    std::string     cors_origin = "*";

    void ApplyCors(httplib::Response& res) const {
        res.set_header("Access-Control-Allow-Origin",  cors_origin);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, DELETE");
        res.set_header("Access-Control-Allow-Headers",
                       "Authorization, Content-Type, X-Requested-With, "
                       "anthropic-version, anthropic-beta, X-Api-Key, X-Stainless-Lang");
        res.set_header("Access-Control-Max-Age", "86400");
    }
};

// ============================================================
// OmniServer
// ============================================================
OmniServer::OmniServer(const OmniServerConfig& cfg)
    : cfg_(cfg),
      http_server_(std::make_unique<HTTPServer>()),
      engine_(std::make_unique<OmniEngine>()),
      tool_calling_(std::make_unique<ToolCallingInterface>()) {
    http_server_->cors_origin = cfg.cors_origin;
}

OmniServer::~OmniServer() {
    Stop();
}

namespace {

// Authenticate a request against the configured bearer key. Empty key means
// auth is disabled.
bool CheckAuth(const std::string& configured_key, const httplib::Request& req) {
    if (configured_key.empty()) return true;
    // Accept either OpenAI style `Authorization: Bearer xxx` or Anthropic
    // style `x-api-key: xxx`. Whichever the client sends is fine.
    if (req.has_header("Authorization")) {
        const auto auth = req.get_header_value("Authorization");
        const std::string prefix = "Bearer ";
        if (auth.size() > prefix.size() &&
            auth.compare(0, prefix.size(), prefix) == 0 &&
            auth.substr(prefix.size()) == configured_key) {
            return true;
        }
    }
    if (req.has_header("x-api-key")) {
        if (req.get_header_value("x-api-key") == configured_key) {
            return true;
        }
    }
    return false;
}

void DenyUnauthorized(httplib::Response& res, const std::string& body_type) {
    res.status = 401;
    if (body_type == "anthropic") {
        res.set_content(api::anthropic::BuildError("missing or invalid api key",
                                                   "authentication_error").dump(),
                        "application/json");
    } else {
        res.set_content(api::openai::BuildError("missing or invalid api key",
                                                "authentication_error",
                                                "invalid_api_key").dump(),
                        "application/json");
    }
}

json SafeParseJson(const std::string& body) {
    if (body.empty()) {
        return json::object();
    }
    try {
        return json::parse(body);
    } catch (const std::exception& e) {
        throw std::invalid_argument(std::string("malformed JSON: ") + e.what());
    }
}

}  // namespace

bool OmniServer::Start() {
    std::cout << "[OmniServer] Starting on port " << cfg_.port
              << (cfg_.mock_mode ? " [MOCK]" : "") << std::endl;

    if (!cfg_.mock_mode) {
        if (!engine_->InitializeBackend()) {
            std::cerr << "[OmniServer] Failed to initialize backend; falling back to mock mode."
                      << std::endl;
            cfg_.mock_mode = true;
        } else {
            engine_->PrintHardwareReport();
        }
    }

    SetupRoutes();

    // Listen on a background thread so callers can issue Stop() / introspect
    // server state without blocking on the network loop.
    server_thread_ = std::thread([this]() {
        http_server_->server.listen("0.0.0.0", cfg_.port);
    });

    // Wait briefly for the listener to come up before reporting success.
    for (int i = 0; i < 50; ++i) {
        if (http_server_->server.is_running()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    is_running_.store(http_server_->server.is_running());

    if (is_running_.load()) {
        std::cout << "[OmniServer] Listening on http://0.0.0.0:" << cfg_.port << std::endl;
        std::cout << "[OmniServer]   - GET  /health" << std::endl;
        std::cout << "[OmniServer]   - GET  /v1/models" << std::endl;
        std::cout << "[OmniServer]   - POST /v1/chat/completions" << std::endl;
        std::cout << "[OmniServer]   - POST /v1/completions" << std::endl;
        std::cout << "[OmniServer]   - POST /v1/embeddings" << std::endl;
        std::cout << "[OmniServer]   - POST /v1/messages" << std::endl;
        std::cout << "[OmniServer]   - POST /v1/models/load" << std::endl;
        std::cout << "[OmniServer]   - POST /tool/call" << std::endl;
        std::cout << "[OmniServer]   - GET  /quantization/status" << std::endl;
    } else {
        std::cerr << "[OmniServer] Failed to bind port " << cfg_.port << std::endl;
    }
    return is_running_.load();
}

bool OmniServer::Stop() {
    if (!is_running_.load()) {
        if (server_thread_.joinable()) server_thread_.join();
        return true;
    }
    http_server_->server.stop();
    if (server_thread_.joinable()) server_thread_.join();
    is_running_.store(false);
    std::cout << "[OmniServer] Stopped" << std::endl;
    return true;
}

// ============================================================
// Routes
// ============================================================
void OmniServer::SetupRoutes() {
    auto& srv = http_server_->server;

    // Helper: wrap a non-streaming JSON handler with auth + CORS + standard
    // error handling. The body type controls the error envelope shape.
    auto wrap_json = [this](const std::string& body_type,
                            std::function<json(const httplib::Request&, const json&)> handler) {
        return [this, body_type, h = std::move(handler)](
                   const httplib::Request& req, httplib::Response& res) {
            http_server_->ApplyCors(res);
            if (!CheckAuth(cfg_.api_key, req)) {
                DenyUnauthorized(res, body_type);
                return;
            }
            try {
                json body = SafeParseJson(req.body);
                json result = h(req, body);
                res.status = 200;
                res.set_content(result.dump(), "application/json");
            } catch (const std::invalid_argument& e) {
                res.status = 400;
                if (body_type == "anthropic") {
                    res.set_content(api::anthropic::BuildError(e.what(), "invalid_request_error").dump(),
                                    "application/json");
                } else {
                    res.set_content(api::openai::BuildError(e.what(),
                                                            "invalid_request_error",
                                                            "bad_request").dump(),
                                    "application/json");
                }
            } catch (const std::exception& e) {
                res.status = 500;
                if (body_type == "anthropic") {
                    res.set_content(api::anthropic::BuildError(e.what(), "internal_error").dump(),
                                    "application/json");
                } else {
                    res.set_content(api::openai::BuildError(e.what(),
                                                            "internal_error",
                                                            "internal_error").dump(),
                                    "application/json");
                }
            }
        };
    };

    // --- CORS preflight: catch-all OPTIONS handler ----------------------
    srv.Options(R"(.*)", [this](const httplib::Request&, httplib::Response& res) {
        http_server_->ApplyCors(res);
        res.status = 204;
    });

    // --- Health ----------------------------------------------------------
    srv.Get("/health", wrap_json("openai", [this](const httplib::Request&, const json&) {
        return HandleHealthCheck();
    }));

    // --- Models list -----------------------------------------------------
    srv.Get("/v1/models", wrap_json("openai", [this](const httplib::Request&, const json&) {
        return HandleModels();
    }));

    // --- Load model ------------------------------------------------------
    srv.Post("/v1/models/load", wrap_json("openai",
        [this](const httplib::Request&, const json& body) {
            return HandleLoadModel(body);
        }));

    // --- OpenAI chat completions (streaming + non-streaming) ------------
    srv.Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& res) {
        http_server_->ApplyCors(res);
        if (!CheckAuth(cfg_.api_key, req)) {
            DenyUnauthorized(res, "openai");
            return;
        }
        json body;
        try {
            body = SafeParseJson(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(api::openai::BuildError(e.what(), "invalid_request_error",
                                                    "bad_request").dump(),
                            "application/json");
            return;
        }
        const bool streaming = body.value("stream", false);
        if (streaming) {
            res.set_chunked_content_provider(
                "text/event-stream",
                [this, body](size_t /*offset*/, httplib::DataSink& sink) {
                    auto wrap = [&sink](const char* data, size_t len) -> bool {
                        return sink.write(data, len);
                    };
                    json out;
                    HandleOpenAIChatCompletion(body, out, wrap);
                    sink.done();
                    return true;
                });
            return;
        }
        json out;
        try {
            HandleOpenAIChatCompletion(body, out, /*sink=*/nullptr);
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::invalid_argument& e) {
            res.status = 400;
            res.set_content(api::openai::BuildError(e.what(), "invalid_request_error",
                                                    "bad_request").dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(api::openai::BuildError(e.what(), "internal_error",
                                                    "internal_error").dump(),
                            "application/json");
        }
    });

    // --- OpenAI legacy completions --------------------------------------
    srv.Post("/v1/completions", [this](const httplib::Request& req, httplib::Response& res) {
        http_server_->ApplyCors(res);
        if (!CheckAuth(cfg_.api_key, req)) {
            DenyUnauthorized(res, "openai");
            return;
        }
        json body;
        try {
            body = SafeParseJson(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(api::openai::BuildError(e.what(), "invalid_request_error",
                                                    "bad_request").dump(),
                            "application/json");
            return;
        }
        try {
            json out;
            HandleOpenAICompletion(body, out, nullptr);
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::invalid_argument& e) {
            res.status = 400;
            res.set_content(api::openai::BuildError(e.what(), "invalid_request_error",
                                                    "bad_request").dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(api::openai::BuildError(e.what(), "internal_error",
                                                    "internal_error").dump(),
                            "application/json");
        }
    });

    // --- Embeddings (always 501 in Stage A; flips to real once an
    //     embedding model is wired through OmniEngine) ------------------
    srv.Post("/v1/embeddings", wrap_json("openai",
        [this](const httplib::Request&, const json& body) {
            return HandleEmbeddings(body);
        }));

    // --- Anthropic messages (streaming + non-streaming) ------------------
    srv.Post("/v1/messages", [this](const httplib::Request& req, httplib::Response& res) {
        http_server_->ApplyCors(res);
        if (!CheckAuth(cfg_.api_key, req)) {
            DenyUnauthorized(res, "anthropic");
            return;
        }
        json body;
        try {
            body = SafeParseJson(req.body);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(api::anthropic::BuildError(e.what(), "invalid_request_error").dump(),
                            "application/json");
            return;
        }
        const bool streaming = body.value("stream", false);
        if (streaming) {
            res.set_chunked_content_provider(
                "text/event-stream",
                [this, body](size_t /*offset*/, httplib::DataSink& sink) {
                    auto wrap = [&sink](const char* data, size_t len) -> bool {
                        return sink.write(data, len);
                    };
                    json out;
                    HandleAnthropicMessages(body, out, wrap);
                    sink.done();
                    return true;
                });
            return;
        }
        json out;
        try {
            HandleAnthropicMessages(body, out, nullptr);
            res.status = 200;
            res.set_content(out.dump(), "application/json");
        } catch (const std::invalid_argument& e) {
            res.status = 400;
            res.set_content(api::anthropic::BuildError(e.what(), "invalid_request_error").dump(),
                            "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(api::anthropic::BuildError(e.what(), "internal_error").dump(),
                            "application/json");
        }
    });

    // --- Tool call -------------------------------------------------------
    srv.Post("/tool/call", wrap_json("openai",
        [this](const httplib::Request&, const json& body) {
            return HandleToolCall(body);
        }));

    // --- Quantization status --------------------------------------------
    srv.Get("/quantization/status", wrap_json("openai",
        [this](const httplib::Request&, const json&) {
            return HandleQuantizationStatus();
        }));
}

// ============================================================
// Non-streaming handlers (existing behaviour, lightly rewritten)
// ============================================================
json OmniServer::HandleHealthCheck() {
    return {
        {"status",         "ok"},
        {"version",        "2.0.0"},
        {"mock_mode",      cfg_.mock_mode},
        {"gpu_available",  !cfg_.mock_mode &&
                           engine_->GetActiveBackend() != HardwareBackend::CPU_FALLBACK},
        {"timestamp",      std::time(nullptr)},
    };
}

json OmniServer::HandleModels() {
    std::vector<std::string> ids;
    if (cfg_.mock_mode || !engine_->IsModelLoaded()) {
        ids.push_back(cfg_.default_model_id);
    } else {
        ids.push_back("omni-inference-2.0");
    }
    return api::openai::SerializeModelsList(ids);
}

json OmniServer::HandleLoadModel(const json& request) {
    if (cfg_.mock_mode) {
        return {{"status", "ignored"}, {"reason", "server is in --mock mode"}};
    }
    std::string model_path = request.value("model_path", "");
    if (model_path.empty()) {
        throw std::invalid_argument("model_path required");
    }

    ModelParameters params;
    params.model_path   = model_path;
    params.model_name   = request.value("model_name", "default");
    params.n_ctx        = request.value("n_ctx", 4096);
    params.n_batch      = request.value("n_batch", 512);
    params.n_gpu_layers = request.value("n_gpu_layers", -1);

    TurboQuantConfig quant_cfg;
    quant_cfg.mode       = QuantizationMode::TURBO_QUANT_3BIT;
    quant_cfg.key_bits   = request.value("key_bits", 4);
    quant_cfg.value_bits = request.value("value_bits", 2);

    if (engine_->LoadModel(params, quant_cfg)) {
        return {
            {"status",  "success"},
            {"model",   params.model_name},
            {"loaded",  true},
        };
    }
    throw std::runtime_error("failed to load model");
}

json OmniServer::HandleEmbeddings(const json& request) {
    // Parse the request to surface 400s on malformed input. Once an actual
    // embedding model is wired through OmniEngine this becomes the real
    // codepath.
    auto parsed = api::openai::ParseEmbeddingsRequest(request);
    (void)parsed;
    return api::openai::BuildEmbeddingsNotImplemented(
        "Pass --embedding-model <gguf> on the OmniServer CLI to enable.");
}

json OmniServer::HandleToolCall(const json& request) {
    std::string tool_name = request.value("tool_name", "");
    if (tool_name.empty()) {
        throw std::invalid_argument("tool_name required");
    }
    json arguments = request.value("arguments", json::object());

    FunctionCall call;
    call.tool_name = tool_name;
    call.arguments = arguments;

    auto result = tool_calling_->ExecuteTool(call);

    return {
        {"call_id",   call.call_id},
        {"tool_name", tool_name},
        {"result",    result.result},
        {"success",   result.success},
    };
}

json OmniServer::HandleQuantizationStatus() {
    auto quant_cfg = engine_->GetCurrentQuantConfig();
    return {
        {"mode",                static_cast<int>(quant_cfg.mode)},
        {"key_bits",            quant_cfg.key_bits},
        {"value_bits",          quant_cfg.value_bits},
        {"use_polar_transform", quant_cfg.use_polar_transform},
        {"use_qjl_correction",  quant_cfg.use_qjl_correction},
        {"active",              !cfg_.mock_mode && engine_->IsModelLoaded()},
    };
}

// ============================================================
// Engine glue: route requests to MockEngine or OmniEngine
// ============================================================
namespace {

std::string FlattenChatPrompt(const api::ChatRequest& req) {
    std::string acc;
    if (!req.params.system_prompt.empty()) {
        acc.append("System: ").append(req.params.system_prompt).append("\n");
    }
    for (const auto& msg : req.messages) {
        const char* tag = "User";
        switch (msg.role) {
            case api::Role::kSystem:    tag = "System";    break;
            case api::Role::kUser:      tag = "User";      break;
            case api::Role::kAssistant: tag = "Assistant"; break;
            case api::Role::kTool:      tag = "Tool";      break;
        }
        acc.append(tag).append(": ");
        for (const auto& part : msg.content) {
            if (part.kind == api::ContentPart::Kind::kText) {
                acc.append(part.text);
            } else if (part.kind == api::ContentPart::Kind::kToolResult) {
                acc.append(part.text);
            }
        }
        acc.append("\n");
    }
    acc.append("Assistant:");
    return acc;
}

}  // namespace

void OmniServer::RunChat(const api::ChatRequest& req,
                         api::ChatResponse& out_response) {
    if (cfg_.mock_mode || !engine_->IsModelLoaded()) {
        out_response       = api::MockEngine::Run(req);
        out_response.model = req.model.empty() ? cfg_.default_model_id : req.model;
        return;
    }
    GenerationConfig gen_cfg;
    gen_cfg.temperature = req.params.temperature;
    gen_cfg.top_p       = req.params.top_p;
    gen_cfg.top_k       = static_cast<float>(req.params.top_k);
    gen_cfg.max_tokens  = req.params.max_tokens;

    std::string prompt = FlattenChatPrompt(req);
    std::string text   = engine_->Generate(prompt, gen_cfg);

    out_response.id                = "chatcmpl-" + std::to_string(std::time(nullptr));
    out_response.model             = req.model.empty() ? "omni-inference-2.0" : req.model;
    out_response.content           = std::move(text);
    out_response.finish_reason     = api::FinishReason::kStop;
    out_response.prompt_tokens     = static_cast<int>(prompt.size() / 4);
    out_response.completion_tokens = static_cast<int>(out_response.content.size() / 4);
}

void OmniServer::StreamChat(const api::ChatRequest& req,
                            const api::StreamCallback& cb) {
    if (cfg_.mock_mode || !engine_->IsModelLoaded()) {
        api::MockEngine::Stream(req, cb);
        return;
    }
    // Real-engine streaming: drive OmniEngine::Generate with its token
    // callback and adapt each emitted fragment to a StreamChunk::kTextDelta.
    GenerationConfig gen_cfg;
    gen_cfg.temperature = req.params.temperature;
    gen_cfg.top_p       = req.params.top_p;
    gen_cfg.top_k       = static_cast<float>(req.params.top_k);
    gen_cfg.max_tokens  = req.params.max_tokens;

    std::string prompt = FlattenChatPrompt(req);

    // Emit the initial start chunk so OpenAI clients see the assistant role
    // immediately.
    {
        api::StreamChunk start;
        start.kind = api::StreamChunk::Kind::kStart;
        cb(start);
    }

    std::string acc;
    engine_->Generate(prompt, gen_cfg, [&](const std::string& fragment) {
        api::StreamChunk delta;
        delta.kind       = api::StreamChunk::Kind::kTextDelta;
        delta.text_delta = fragment;
        acc.append(fragment);
        cb(delta);
    });

    api::StreamChunk fin;
    fin.kind          = api::StreamChunk::Kind::kFinish;
    fin.finish_reason = api::FinishReason::kStop;
    fin.prompt_tokens = static_cast<int>(prompt.size() / 4);
    fin.completion_tokens = static_cast<int>(acc.size() / 4);
    cb(fin);
}

// ============================================================
// OpenAI chat completion (with streaming)
// ============================================================
bool OmniServer::HandleOpenAIChatCompletion(const json& body,
                                            json& out_response,
                                            StreamSink sink) {
    api::ChatRequest req = api::openai::ParseChatRequest(body);
    if (req.model.empty()) {
        req.model = cfg_.default_model_id;
    }
    // Decode any image_url content parts into raw RGB pixels so the engine
    // (or the mock acknowledgement string) doesn't have to re-implement
    // base64 + stb_image. Decode failures are surfaced via the part's
    // `image_decode_error`; we deliberately do NOT 4xx the whole request
    // because OpenAI semantics say the model should still respond.
    api::DecodeImagesIn(req);

    if (req.params.stream && sink) {
        const std::string response_id = "chatcmpl-" + std::to_string(std::time(nullptr));
        const std::string model_id    = req.model;
        StreamChat(req, [&](const api::StreamChunk& chunk) {
            if (chunk.kind == api::StreamChunk::Kind::kStart) {
                api::sse::EmitOpenAI(sink,
                    api::openai::SerializeStreamChunk(chunk, response_id, model_id));
                return;
            }
            api::sse::EmitOpenAI(sink,
                api::openai::SerializeStreamChunk(chunk, response_id, model_id));
        });
        api::sse::EmitOpenAIDone(sink);
        return true;
    }

    // Non-streaming path
    api::ChatResponse resp;
    RunChat(req, resp);
    out_response = api::openai::SerializeChatResponse(resp);
    return false;
}

bool OmniServer::HandleOpenAICompletion(const json& body,
                                        json& out_response,
                                        StreamSink /*sink*/) {
    auto legacy = api::openai::ParseLegacyCompletionRequest(body);

    // Reuse the chat path: a legacy completion is just a chat with one user
    // message containing the prompt.
    api::ChatRequest req;
    req.model           = legacy.model.empty() ? cfg_.default_model_id : legacy.model;
    req.params          = legacy.params;
    req.params.stream   = false;  // legacy /v1/completions streaming not in scope
    api::Message msg;
    msg.role = api::Role::kUser;
    api::ContentPart part;
    part.kind = api::ContentPart::Kind::kText;
    part.text = legacy.prompt;
    msg.content.push_back(std::move(part));
    req.messages.push_back(std::move(msg));

    api::ChatResponse resp;
    RunChat(req, resp);
    out_response = api::openai::SerializeLegacyCompletionResponse(legacy, resp);
    return false;
}

// ============================================================
// Anthropic messages (with streaming)
// ============================================================
bool OmniServer::HandleAnthropicMessages(const json& body,
                                         json& out_response,
                                         StreamSink sink) {
    api::ChatRequest req = api::anthropic::ParseMessagesRequest(body);
    if (req.model.empty()) {
        req.model = cfg_.default_model_id;
    }
    // Same decode-then-pass pattern as the OpenAI path. Anthropic image
    // sources (base64 or URL) have already been collapsed to a `data:` URI
    // by ParseMessagesRequest, so we just hit the unified decoder here.
    api::DecodeImagesIn(req);

    if (req.params.stream && sink) {
        const std::string message_id = "msg_" + std::to_string(std::time(nullptr));
        const std::string model_id   = req.model;

        // Anthropic's stream is much chattier: message_start, then per-block
        // start/delta/stop, then message_delta + message_stop. We emit the
        // first content block lazily — for tool-only responses we never open
        // a text block at all.
        bool text_block_open = false;
        int  next_block_idx  = 0;
        int  current_text_block = -1;
        int  output_tokens   = 0;

        auto emit = [&](const api::anthropic::EventEnvelope& env) {
            api::sse::EmitAnthropic(sink, env.event, env.data);
        };

        emit(api::anthropic::BuildMessageStart(message_id, model_id));

        api::FinishReason finish_reason = api::FinishReason::kStop;

        StreamChat(req, [&](const api::StreamChunk& chunk) {
            switch (chunk.kind) {
                case api::StreamChunk::Kind::kStart:
                    // Anthropic doesn't have a per-message start delta; drop.
                    break;
                case api::StreamChunk::Kind::kTextDelta:
                    if (!text_block_open) {
                        current_text_block = next_block_idx++;
                        emit(api::anthropic::BuildContentBlockStartText(current_text_block));
                        text_block_open = true;
                    }
                    emit(api::anthropic::BuildContentBlockDeltaText(current_text_block, chunk.text_delta));
                    output_tokens += static_cast<int>((chunk.text_delta.size() + 3) / 4);
                    break;
                case api::StreamChunk::Kind::kToolCallStart: {
                    if (text_block_open) {
                        emit(api::anthropic::BuildContentBlockStop(current_text_block));
                        text_block_open = false;
                    }
                    int block_idx = next_block_idx++;
                    emit(api::anthropic::BuildContentBlockStartToolUse(
                        block_idx, chunk.tool_call_id, chunk.tool_call_name));
                    break;
                }
                case api::StreamChunk::Kind::kToolCallArgumentsDelta: {
                    // The active tool-use block is always the most recent one;
                    // index = next_block_idx - 1.
                    int block_idx = next_block_idx - 1;
                    emit(api::anthropic::BuildContentBlockDeltaToolUseInput(
                        block_idx, chunk.tool_call_arguments_delta));
                    output_tokens += static_cast<int>((chunk.tool_call_arguments_delta.size() + 3) / 4);
                    break;
                }
                case api::StreamChunk::Kind::kFinish:
                    finish_reason = chunk.finish_reason;
                    if (chunk.completion_tokens > 0) {
                        output_tokens = chunk.completion_tokens;
                    }
                    break;
            }
        });

        if (text_block_open) {
            emit(api::anthropic::BuildContentBlockStop(current_text_block));
            text_block_open = false;
        }
        // Close any tool_use blocks that were opened but never closed by an
        // explicit stop chunk. Right now StreamChat never opens more than one
        // unclosed block at a time, so checking next_block_idx > 0 with no
        // text block is sufficient.
        if (!text_block_open && next_block_idx > 0 && finish_reason == api::FinishReason::kToolCalls) {
            emit(api::anthropic::BuildContentBlockStop(next_block_idx - 1));
        }
        emit(api::anthropic::BuildMessageDelta(finish_reason, output_tokens));
        emit(api::anthropic::BuildMessageStop());
        return true;
    }

    api::ChatResponse resp;
    RunChat(req, resp);
    out_response = api::anthropic::SerializeMessageResponse(resp);
    return false;
}
