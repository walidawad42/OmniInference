// Internal, API-agnostic chat request / response types used by the
// OpenAI-compatible and Anthropic-compatible HTTP front-ends. Each front-end
// translates the wire format to/from these structs so the engine never sees
// vendor-specific JSON shapes.
//
// The naming roughly follows the OpenAI vocabulary because that's the more
// granular / older API; Anthropic concepts (`tool_use` blocks, etc.) are
// flattened into this representation.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace omni::api {

// Forward-declare the decoded image type so ContentPart can hold a
// shared_ptr to one without dragging in stb_image. The full definition
// lives in api/image_decode.h; only TUs that actually want to *look at*
// the pixels need to include it.
struct DecodedImage;

enum class Role {
    kSystem,
    kUser,
    kAssistant,
    kTool,
};

const char* RoleToString(Role role);
Role        RoleFromString(const std::string& s);  // throws on unknown input

// A single content part inside a message. OpenAI represents multimodal input
// as `content: [{type:"text"|"image_url", ...}, ...]`; Anthropic uses a
// similar `content: [{type:"text"|"image"|"tool_use"|"tool_result", ...}]`
// list. We unify them here.
struct ContentPart {
    enum class Kind {
        kText,
        kImageUrl,         // OpenAI: image_url.url, Anthropic: image.source.data
        kToolUse,          // assistant requests a tool call (Anthropic style)
        kToolResult,       // user/tool returns a tool result (Anthropic style)
    };

    Kind        kind = Kind::kText;
    std::string text;                 // for kText, kToolResult
    std::string image_url;            // for kImageUrl (URL or `data:` URI)
    std::string image_media_type;     // for kImageUrl (e.g. "image/png")
    std::string tool_use_id;          // for kToolUse and kToolResult
    std::string tool_name;            // for kToolUse
    nlohmann::json tool_arguments;    // for kToolUse, parsed JSON object
    bool        tool_result_is_error = false;  // for kToolResult

    // Stage B: lazily-attached decoded pixels. Populated by
    // `omni::api::DecodeImagesIn(ChatRequest&)` (image_decode.h) before the
    // request reaches the engine, or left null if decode was skipped /
    // failed. `image_decode_error` carries the failure reason for surfacing
    // back to the caller.
    std::shared_ptr<DecodedImage> decoded_image;
    std::string                   image_decode_error;
};

struct Message {
    Role                       role = Role::kUser;
    std::vector<ContentPart>   content;
    std::string                name;            // for tool messages: tool name
    std::string                tool_call_id;    // for tool messages: id being answered
};

struct ToolDefinition {
    std::string    name;
    std::string    description;
    nlohmann::json parameters_schema;  // JSON Schema (object)
};

struct GenerationParameters {
    float       temperature       = 0.7f;
    float       top_p             = 0.95f;
    int         top_k             = 40;
    int         max_tokens        = 512;
    bool        stream            = false;
    std::vector<std::string> stop;
    int64_t     seed              = -1;        // -1 = random
    // Anthropic supports a top-level system field; OpenAI puts the system
    // message in the messages list. Either way it ends up in this string
    // (or empty if there's no system prompt).
    std::string system_prompt;
};

struct ChatRequest {
    std::string                model;            // logical model id
    std::vector<Message>       messages;
    std::vector<ToolDefinition> tools;
    GenerationParameters       params;
    // tool_choice: "auto", "none", "required", or {"type":"tool","name":"..."}
    nlohmann::json             tool_choice = "auto";
};

enum class FinishReason {
    kStop,           // model emitted stop token / end of turn
    kMaxTokens,      // hit max_tokens
    kToolCalls,      // model wants to call a tool
    kContentFilter,  // unused for now, reserved
    kError,
};

const char* FinishReasonToString(FinishReason r);

struct ToolCall {
    std::string    id;
    std::string    name;
    nlohmann::json arguments;
};

struct ChatResponse {
    std::string             id;          // request/response id
    std::string             model;       // echo of request.model
    std::string             content;     // assistant text content
    std::vector<ToolCall>   tool_calls;
    FinishReason            finish_reason = FinishReason::kStop;
    int                     prompt_tokens     = 0;
    int                     completion_tokens = 0;
    std::chrono::system_clock::time_point created
        = std::chrono::system_clock::now();
};

// Streaming delta. Each chunk carries the *new* text (or a partial tool call)
// produced since the last chunk. The front-end is responsible for translating
// these deltas into the wire format expected by the client (OpenAI's
// `choices[0].delta.content` vs Anthropic's `content_block_delta`).
struct StreamChunk {
    enum class Kind {
        kStart,                  // very first chunk, before any content
        kTextDelta,              // additional text
        kToolCallStart,          // model started emitting a tool call
        kToolCallArgumentsDelta, // partial JSON for the latest tool call
        kFinish,                 // finish reason set, no more chunks
    };

    Kind         kind = Kind::kStart;
    std::string  text_delta;
    // tool call fields (used for kToolCallStart / kToolCallArgumentsDelta /
    // kFinish when finish reason is kToolCalls)
    int          tool_call_index = -1;
    std::string  tool_call_id;
    std::string  tool_call_name;
    std::string  tool_call_arguments_delta;  // partial JSON fragment

    FinishReason finish_reason = FinishReason::kStop;  // valid when kind == kFinish
    int          prompt_tokens     = 0;                // valid when kind == kFinish
    int          completion_tokens = 0;                // valid when kind == kFinish
};

using StreamCallback = std::function<void(const StreamChunk&)>;

}  // namespace omni::api
