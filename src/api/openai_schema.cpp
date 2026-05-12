#include "api/openai_schema.h"

#include <chrono>
#include <stdexcept>

#include "api/tool_translator.h"

namespace omni::api {

const char* RoleToString(Role role) {
    switch (role) {
        case Role::kSystem:    return "system";
        case Role::kUser:      return "user";
        case Role::kAssistant: return "assistant";
        case Role::kTool:      return "tool";
    }
    return "user";
}

Role RoleFromString(const std::string& s) {
    if (s == "system")    return Role::kSystem;
    if (s == "user")      return Role::kUser;
    if (s == "assistant") return Role::kAssistant;
    if (s == "tool" || s == "function") return Role::kTool;
    throw std::invalid_argument("unknown role: " + s);
}

const char* FinishReasonToString(FinishReason r) {
    switch (r) {
        case FinishReason::kStop:          return "stop";
        case FinishReason::kMaxTokens:     return "length";
        case FinishReason::kToolCalls:     return "tool_calls";
        case FinishReason::kContentFilter: return "content_filter";
        case FinishReason::kError:         return "error";
    }
    return "stop";
}

}  // namespace omni::api

namespace omni::api::openai {

namespace {

// Convert the OpenAI `content` field to a list of ContentParts. The field
// can be either a plain string or an array of content parts.
std::vector<ContentPart> ParseContent(const nlohmann::json& content) {
    std::vector<ContentPart> out;
    if (content.is_string()) {
        ContentPart part;
        part.kind = ContentPart::Kind::kText;
        part.text = content.get<std::string>();
        out.push_back(std::move(part));
        return out;
    }
    if (!content.is_array()) {
        return out;  // null / missing -> no content
    }
    for (const auto& item : content) {
        if (item.is_string()) {
            ContentPart part;
            part.kind = ContentPart::Kind::kText;
            part.text = item.get<std::string>();
            out.push_back(std::move(part));
            continue;
        }
        if (!item.is_object()) {
            continue;
        }
        const std::string type = item.value("type", "text");
        if (type == "text") {
            ContentPart part;
            part.kind = ContentPart::Kind::kText;
            part.text = item.value("text", "");
            out.push_back(std::move(part));
        } else if (type == "image_url") {
            ContentPart part;
            part.kind = ContentPart::Kind::kImageUrl;
            if (item.contains("image_url") && item["image_url"].is_object()) {
                part.image_url = item["image_url"].value("url", "");
            } else {
                part.image_url = item.value("image_url", "");
            }
            // OpenAI doesn't ship the media type as a separate field — it's
            // baked into the `data:<mime>;base64,...` URL. Sniff it now so
            // the rest of the pipeline (vision encoder, mock responder, ...)
            // doesn't have to re-parse the URL string.
            if (part.image_url.compare(0, 5, "data:") == 0) {
                const auto comma = part.image_url.find(',');
                if (comma != std::string::npos) {
                    std::string head = part.image_url.substr(5, comma - 5);
                    auto semi = head.find(';');
                    part.image_media_type = (semi == std::string::npos)
                        ? head : head.substr(0, semi);
                }
            }
            out.push_back(std::move(part));
        }
        // Unknown content types are dropped silently — we'd rather forward
        // a partially-typed request than 400 on a part we just don't render.
    }
    return out;
}

nlohmann::json SerializeContentParts(const std::vector<ContentPart>& parts) {
    // For a response the engine only ever produces text parts; collapse them
    // into a single string for OpenAI compatibility.
    std::string acc;
    for (const auto& p : parts) {
        if (p.kind == ContentPart::Kind::kText) {
            acc.append(p.text);
        }
    }
    return acc;
}

}  // namespace

ChatRequest ParseChatRequest(const nlohmann::json& body) {
    if (!body.is_object()) {
        throw std::invalid_argument("request body must be a JSON object");
    }
    ChatRequest req;
    req.model = body.value("model", "");
    if (!body.contains("messages") || !body["messages"].is_array()) {
        throw std::invalid_argument("messages array is required");
    }
    for (const auto& m : body["messages"]) {
        if (!m.is_object() || !m.contains("role")) {
            throw std::invalid_argument("each message needs a role");
        }
        Message msg;
        msg.role = RoleFromString(m["role"].get<std::string>());
        if (m.contains("content")) {
            msg.content = ParseContent(m["content"]);
        }
        msg.name         = m.value("name", "");
        msg.tool_call_id = m.value("tool_call_id", "");

        // Assistant messages may carry tool_calls; attach them as tool_use
        // content parts so the engine sees a unified shape.
        if (m.contains("tool_calls")) {
            auto calls = openai_tools::ParseToolCalls(m["tool_calls"]);
            for (auto& call : calls) {
                ContentPart part;
                part.kind           = ContentPart::Kind::kToolUse;
                part.tool_use_id    = call.id;
                part.tool_name      = call.name;
                part.tool_arguments = call.arguments;
                msg.content.push_back(std::move(part));
            }
        }
        req.messages.push_back(std::move(msg));
    }

    if (body.contains("tools")) {
        req.tools = openai_tools::ParseTools(body["tools"]);
    } else if (body.contains("functions")) {
        // Legacy `functions` field — same shape as a function definition
        // without the outer {"type":"function","function":...} wrapper.
        if (body["functions"].is_array()) {
            for (const auto& fn : body["functions"]) {
                ToolDefinition def;
                def.name              = fn.value("name", "");
                def.description       = fn.value("description", "");
                def.parameters_schema = fn.value("parameters", nlohmann::json::object());
                if (!def.name.empty()) {
                    req.tools.push_back(std::move(def));
                }
            }
        }
    }

    if (body.contains("tool_choice")) {
        req.tool_choice = body["tool_choice"];
    }

    auto& p = req.params;
    p.temperature = body.value("temperature", p.temperature);
    p.top_p       = body.value("top_p",       p.top_p);
    p.top_k       = body.value("top_k",       p.top_k);
    p.max_tokens  = body.value("max_tokens",  p.max_tokens);
    p.stream      = body.value("stream",      p.stream);
    p.seed        = body.value("seed",        p.seed);
    if (body.contains("stop")) {
        const auto& stop = body["stop"];
        if (stop.is_string()) {
            p.stop.push_back(stop.get<std::string>());
        } else if (stop.is_array()) {
            for (const auto& s : stop) {
                if (s.is_string()) p.stop.push_back(s.get<std::string>());
            }
        }
    }

    // Hoist any role:"system" message out into params.system_prompt for
    // engine-side simplicity (matches Anthropic's top-level `system` field).
    for (auto it = req.messages.begin(); it != req.messages.end(); ) {
        if (it->role == Role::kSystem) {
            for (const auto& part : it->content) {
                if (part.kind == ContentPart::Kind::kText) {
                    if (!p.system_prompt.empty()) p.system_prompt.push_back('\n');
                    p.system_prompt.append(part.text);
                }
            }
            it = req.messages.erase(it);
        } else {
            ++it;
        }
    }

    return req;
}

nlohmann::json SerializeChatResponse(const ChatResponse& response) {
    auto created_secs = std::chrono::duration_cast<std::chrono::seconds>(
        response.created.time_since_epoch()).count();

    nlohmann::json message = {
        {"role",    "assistant"},
        {"content", response.content},
    };
    if (!response.tool_calls.empty()) {
        message["tool_calls"] = openai_tools::SerializeToolCalls(response.tool_calls);
    }

    nlohmann::json choice = {
        {"index",         0},
        {"message",       std::move(message)},
        {"finish_reason", FinishReasonToString(response.finish_reason)},
    };

    return {
        {"id",      response.id},
        {"object",  "chat.completion"},
        {"created", created_secs},
        {"model",   response.model},
        {"choices", nlohmann::json::array({std::move(choice)})},
        {"usage", {
            {"prompt_tokens",     response.prompt_tokens},
            {"completion_tokens", response.completion_tokens},
            {"total_tokens",      response.prompt_tokens + response.completion_tokens},
        }},
    };
}

nlohmann::json SerializeStreamChunk(const StreamChunk& chunk,
                                    const std::string& response_id,
                                    const std::string& model) {
    auto created_secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    nlohmann::json delta = nlohmann::json::object();
    nlohmann::json finish = nullptr;

    switch (chunk.kind) {
        case StreamChunk::Kind::kStart:
            delta = {{"role", "assistant"}, {"content", ""}};
            break;
        case StreamChunk::Kind::kTextDelta:
            delta = {{"content", chunk.text_delta}};
            break;
        case StreamChunk::Kind::kToolCallStart: {
            nlohmann::json tc = {
                {"index",    chunk.tool_call_index},
                {"id",       chunk.tool_call_id},
                {"type",     "function"},
                {"function", {{"name", chunk.tool_call_name}, {"arguments", ""}}},
            };
            delta = {{"tool_calls", nlohmann::json::array({std::move(tc)})}};
            break;
        }
        case StreamChunk::Kind::kToolCallArgumentsDelta: {
            nlohmann::json tc = {
                {"index",    chunk.tool_call_index},
                {"function", {{"arguments", chunk.tool_call_arguments_delta}}},
            };
            delta = {{"tool_calls", nlohmann::json::array({std::move(tc)})}};
            break;
        }
        case StreamChunk::Kind::kFinish:
            finish = FinishReasonToString(chunk.finish_reason);
            break;
    }

    nlohmann::json choice = {
        {"index",         0},
        {"delta",         std::move(delta)},
        {"finish_reason", std::move(finish)},
    };

    return {
        {"id",      response_id},
        {"object",  "chat.completion.chunk"},
        {"created", created_secs},
        {"model",   model},
        {"choices", nlohmann::json::array({std::move(choice)})},
    };
}

LegacyCompletionRequest ParseLegacyCompletionRequest(const nlohmann::json& body) {
    if (!body.is_object()) {
        throw std::invalid_argument("request body must be a JSON object");
    }
    LegacyCompletionRequest req;
    req.model  = body.value("model", "");
    req.prompt = body.value("prompt", "");
    auto& p = req.params;
    p.temperature = body.value("temperature", p.temperature);
    p.top_p       = body.value("top_p",       p.top_p);
    p.top_k       = body.value("top_k",       p.top_k);
    p.max_tokens  = body.value("max_tokens",  p.max_tokens);
    p.stream      = body.value("stream",      p.stream);
    p.seed        = body.value("seed",        p.seed);
    if (body.contains("stop")) {
        const auto& stop = body["stop"];
        if (stop.is_string()) {
            p.stop.push_back(stop.get<std::string>());
        } else if (stop.is_array()) {
            for (const auto& s : stop) {
                if (s.is_string()) p.stop.push_back(s.get<std::string>());
            }
        }
    }
    return req;
}

nlohmann::json SerializeLegacyCompletionResponse(const LegacyCompletionRequest& req,
                                                 const ChatResponse& response) {
    auto created_secs = std::chrono::duration_cast<std::chrono::seconds>(
        response.created.time_since_epoch()).count();
    return {
        {"id",      response.id},
        {"object",  "text_completion"},
        {"created", created_secs},
        {"model",   req.model.empty() ? response.model : req.model},
        {"choices", nlohmann::json::array({{
            {"text",          response.content},
            {"index",         0},
            {"logprobs",      nullptr},
            {"finish_reason", FinishReasonToString(response.finish_reason)},
        }})},
        {"usage", {
            {"prompt_tokens",     response.prompt_tokens},
            {"completion_tokens", response.completion_tokens},
            {"total_tokens",      response.prompt_tokens + response.completion_tokens},
        }},
    };
}

EmbeddingsRequest ParseEmbeddingsRequest(const nlohmann::json& body) {
    if (!body.is_object()) {
        throw std::invalid_argument("request body must be a JSON object");
    }
    EmbeddingsRequest req;
    req.model = body.value("model", "");
    if (!body.contains("input")) {
        throw std::invalid_argument("input is required");
    }
    const auto& input = body["input"];
    if (input.is_string()) {
        req.inputs.push_back(input.get<std::string>());
    } else if (input.is_array()) {
        for (const auto& s : input) {
            if (s.is_string()) {
                req.inputs.push_back(s.get<std::string>());
            }
        }
    } else {
        throw std::invalid_argument("input must be string or array of strings");
    }
    return req;
}

nlohmann::json BuildEmbeddingsNotImplemented(const std::string& detail) {
    return BuildError(
        "Embeddings model is not loaded. " + detail,
        "embeddings_not_loaded",
        "no_embedding_model");
}

nlohmann::json SerializeModelsList(const std::vector<std::string>& model_ids) {
    auto created_secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    nlohmann::json data = nlohmann::json::array();
    for (const auto& id : model_ids) {
        data.push_back({
            {"id",       id},
            {"object",   "model"},
            {"created",  created_secs},
            {"owned_by", "omni-inference"},
        });
    }
    return {
        {"object", "list"},
        {"data",   std::move(data)},
    };
}

nlohmann::json BuildError(const std::string& message,
                          const std::string& type,
                          const std::string& code) {
    return {
        {"error", {
            {"message", message},
            {"type",    type},
            {"code",    code},
            {"param",   nullptr},
        }},
    };
}

}  // namespace omni::api::openai
