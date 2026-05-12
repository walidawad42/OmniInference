#include "api/anthropic_schema.h"

#include <chrono>
#include <stdexcept>

#include "api/tool_translator.h"

namespace omni::api::anthropic {

namespace {

std::vector<ContentPart> ParseAnthropicContent(const nlohmann::json& content) {
    std::vector<ContentPart> out;
    if (content.is_string()) {
        ContentPart part;
        part.kind = ContentPart::Kind::kText;
        part.text = content.get<std::string>();
        out.push_back(std::move(part));
        return out;
    }
    if (!content.is_array()) {
        return out;
    }
    for (const auto& item : content) {
        if (!item.is_object()) {
            continue;
        }
        const std::string type = item.value("type", "text");
        if (type == "text") {
            ContentPart part;
            part.kind = ContentPart::Kind::kText;
            part.text = item.value("text", "");
            out.push_back(std::move(part));
        } else if (type == "image") {
            ContentPart part;
            part.kind = ContentPart::Kind::kImageUrl;
            // Anthropic encodes images as either source.type=base64 with
            // base64 data, or source.type=url with a url. Collapse both into
            // image_url so the engine's vision pipeline only has to handle
            // one shape (a URL or a `data:` URI).
            if (item.contains("source") && item["source"].is_object()) {
                const auto& src = item["source"];
                const std::string src_type = src.value("type", "");
                if (src_type == "base64") {
                    part.image_media_type = src.value("media_type", "image/png");
                    part.image_url = "data:" + part.image_media_type + ";base64," +
                                     src.value("data", "");
                } else if (src_type == "url") {
                    part.image_url = src.value("url", "");
                    part.image_media_type = src.value("media_type", "");
                }
            }
            out.push_back(std::move(part));
        } else if (type == "tool_use") {
            ContentPart part;
            part.kind          = ContentPart::Kind::kToolUse;
            part.tool_use_id   = item.value("id", "");
            part.tool_name     = item.value("name", "");
            part.tool_arguments = item.value("input", nlohmann::json::object());
            out.push_back(std::move(part));
        } else if (type == "tool_result") {
            ContentPart part;
            part.kind                 = ContentPart::Kind::kToolResult;
            part.tool_use_id          = item.value("tool_use_id", "");
            part.tool_result_is_error = item.value("is_error", false);
            // `content` in a tool_result can be a string or an array of text
            // blocks; collapse to a single string.
            if (item.contains("content")) {
                const auto& tc = item["content"];
                if (tc.is_string()) {
                    part.text = tc.get<std::string>();
                } else if (tc.is_array()) {
                    for (const auto& sub : tc) {
                        if (sub.is_object() && sub.value("type", "") == "text") {
                            part.text.append(sub.value("text", ""));
                        } else if (sub.is_string()) {
                            part.text.append(sub.get<std::string>());
                        }
                    }
                }
            }
            out.push_back(std::move(part));
        }
    }
    return out;
}

}  // namespace

ChatRequest ParseMessagesRequest(const nlohmann::json& body) {
    if (!body.is_object()) {
        throw std::invalid_argument("request body must be a JSON object");
    }
    ChatRequest req;
    req.model = body.value("model", "");

    // `system` is top-level. Accept either a string or an array of text blocks.
    if (body.contains("system")) {
        const auto& sys = body["system"];
        if (sys.is_string()) {
            req.params.system_prompt = sys.get<std::string>();
        } else if (sys.is_array()) {
            for (const auto& blk : sys) {
                if (blk.is_object() && blk.value("type", "") == "text") {
                    if (!req.params.system_prompt.empty()) req.params.system_prompt.push_back('\n');
                    req.params.system_prompt.append(blk.value("text", ""));
                }
            }
        }
    }

    if (!body.contains("messages") || !body["messages"].is_array()) {
        throw std::invalid_argument("messages array is required");
    }
    for (const auto& m : body["messages"]) {
        if (!m.is_object() || !m.contains("role")) {
            throw std::invalid_argument("each message needs a role");
        }
        Message msg;
        const std::string role = m["role"].get<std::string>();
        if (role == "user") {
            msg.role = Role::kUser;
        } else if (role == "assistant") {
            msg.role = Role::kAssistant;
        } else {
            // Anthropic only accepts user / assistant in the messages array;
            // be lenient and surface anything else as a user turn.
            msg.role = Role::kUser;
        }
        if (m.contains("content")) {
            msg.content = ParseAnthropicContent(m["content"]);
        }

        // Anthropic puts tool results inside user messages as content parts.
        // Hoist the first tool_result we see into Message::tool_call_id so the
        // engine can correlate without traversing content parts. (The full
        // content list still carries the original tool_result block.)
        for (const auto& part : msg.content) {
            if (part.kind == ContentPart::Kind::kToolResult) {
                msg.role = Role::kTool;
                msg.tool_call_id = part.tool_use_id;
                break;
            }
        }
        req.messages.push_back(std::move(msg));
    }

    if (body.contains("tools")) {
        req.tools = anthropic_tools::ParseTools(body["tools"]);
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
    if (body.contains("stop_sequences") && body["stop_sequences"].is_array()) {
        for (const auto& s : body["stop_sequences"]) {
            if (s.is_string()) p.stop.push_back(s.get<std::string>());
        }
    }

    return req;
}

nlohmann::json SerializeMessageResponse(const ChatResponse& response) {
    auto blocks = anthropic_tools::BuildAssistantContentBlocks(
        response.content, response.tool_calls);

    // Anthropic uses different stop_reason vocabulary than OpenAI's
    // finish_reason — translate.
    const char* stop_reason = "end_turn";
    switch (response.finish_reason) {
        case FinishReason::kStop:          stop_reason = "end_turn";      break;
        case FinishReason::kMaxTokens:     stop_reason = "max_tokens";    break;
        case FinishReason::kToolCalls:     stop_reason = "tool_use";      break;
        case FinishReason::kContentFilter: stop_reason = "stop_sequence"; break;
        case FinishReason::kError:         stop_reason = "error";         break;
    }

    return {
        {"id",            response.id},
        {"type",          "message"},
        {"role",          "assistant"},
        {"model",         response.model},
        {"content",       std::move(blocks)},
        {"stop_reason",   stop_reason},
        {"stop_sequence", nullptr},
        {"usage", {
            {"input_tokens",  response.prompt_tokens},
            {"output_tokens", response.completion_tokens},
        }},
    };
}

EventEnvelope BuildMessageStart(const std::string& message_id,
                                const std::string& model) {
    nlohmann::json data = {
        {"type", "message_start"},
        {"message", {
            {"id",            message_id},
            {"type",          "message"},
            {"role",          "assistant"},
            {"model",         model},
            {"content",       nlohmann::json::array()},
            {"stop_reason",   nullptr},
            {"stop_sequence", nullptr},
            {"usage",         {{"input_tokens", 0}, {"output_tokens", 0}}},
        }},
    };
    return {"message_start", std::move(data)};
}

EventEnvelope BuildContentBlockStartText(int block_index) {
    nlohmann::json data = {
        {"type",          "content_block_start"},
        {"index",         block_index},
        {"content_block", {{"type", "text"}, {"text", ""}}},
    };
    return {"content_block_start", std::move(data)};
}

EventEnvelope BuildContentBlockDeltaText(int block_index, const std::string& text_delta) {
    nlohmann::json data = {
        {"type",  "content_block_delta"},
        {"index", block_index},
        {"delta", {{"type", "text_delta"}, {"text", text_delta}}},
    };
    return {"content_block_delta", std::move(data)};
}

EventEnvelope BuildContentBlockStartToolUse(int block_index,
                                            const std::string& tool_use_id,
                                            const std::string& tool_name) {
    nlohmann::json data = {
        {"type",          "content_block_start"},
        {"index",         block_index},
        {"content_block", {
            {"type",  "tool_use"},
            {"id",    tool_use_id},
            {"name",  tool_name},
            {"input", nlohmann::json::object()},
        }},
    };
    return {"content_block_start", std::move(data)};
}

EventEnvelope BuildContentBlockDeltaToolUseInput(int block_index,
                                                 const std::string& partial_json) {
    nlohmann::json data = {
        {"type",  "content_block_delta"},
        {"index", block_index},
        {"delta", {{"type", "input_json_delta"}, {"partial_json", partial_json}}},
    };
    return {"content_block_delta", std::move(data)};
}

EventEnvelope BuildContentBlockStop(int block_index) {
    nlohmann::json data = {
        {"type",  "content_block_stop"},
        {"index", block_index},
    };
    return {"content_block_stop", std::move(data)};
}

EventEnvelope BuildMessageDelta(FinishReason finish_reason, int output_tokens) {
    const char* stop_reason = "end_turn";
    switch (finish_reason) {
        case FinishReason::kStop:          stop_reason = "end_turn";      break;
        case FinishReason::kMaxTokens:     stop_reason = "max_tokens";    break;
        case FinishReason::kToolCalls:     stop_reason = "tool_use";      break;
        case FinishReason::kContentFilter: stop_reason = "stop_sequence"; break;
        case FinishReason::kError:         stop_reason = "error";         break;
    }
    nlohmann::json data = {
        {"type",  "message_delta"},
        {"delta", {
            {"stop_reason",   stop_reason},
            {"stop_sequence", nullptr},
        }},
        {"usage", {{"output_tokens", output_tokens}}},
    };
    return {"message_delta", std::move(data)};
}

EventEnvelope BuildMessageStop() {
    return {"message_stop", {{"type", "message_stop"}}};
}

nlohmann::json BuildError(const std::string& message,
                          const std::string& type) {
    return {
        {"type",  "error"},
        {"error", {{"type", type}, {"message", message}}},
    };
}

}  // namespace omni::api::anthropic
