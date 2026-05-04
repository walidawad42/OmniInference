// Wire-format <-> internal translation for the Anthropic Messages API.
//
// Reference: https://docs.anthropic.com/en/api/messages
//
// Notable differences from the OpenAI Chat Completions schema:
//
//   * `system` is a top-level field, not a message with role:"system".
//   * `messages` only contains user / assistant turns; tool results live
//     inside user messages as content blocks of type "tool_result".
//   * Tool calls live inside assistant messages as content blocks of type
//     "tool_use" rather than a separate `tool_calls` array.
//   * Streaming uses an explicit event stream:
//       message_start -> content_block_start -> content_block_delta* ->
//       content_block_stop -> message_delta -> message_stop
//     rather than a single `chat.completion.chunk` shape.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "api/chat_request.h"

namespace omni::api::anthropic {

// Parse an Anthropic `/v1/messages` POST body into our internal ChatRequest.
// Throws std::invalid_argument on malformed input.
ChatRequest ParseMessagesRequest(const nlohmann::json& body);

// Serialise a non-streaming ChatResponse as Anthropic's `message` object.
nlohmann::json SerializeMessageResponse(const ChatResponse& response);

// Streaming has multiple distinct event types. Each helper returns the JSON
// object that goes after `data: ` on the wire; the SSE framer adds the
// `event:` line above it.
struct EventEnvelope {
    std::string    event;  // e.g. "message_start"
    nlohmann::json data;
};

EventEnvelope BuildMessageStart(const std::string& message_id,
                                const std::string& model);

// Anthropic emits a `content_block_start` for each block (text or tool_use)
// then a sequence of `content_block_delta` events, then a
// `content_block_stop`. block_index is 0-based.
EventEnvelope BuildContentBlockStartText(int block_index);
EventEnvelope BuildContentBlockDeltaText(int block_index, const std::string& text_delta);

EventEnvelope BuildContentBlockStartToolUse(int block_index,
                                            const std::string& tool_use_id,
                                            const std::string& tool_name);
EventEnvelope BuildContentBlockDeltaToolUseInput(int block_index,
                                                 const std::string& partial_json);

EventEnvelope BuildContentBlockStop(int block_index);

EventEnvelope BuildMessageDelta(FinishReason finish_reason,
                                int output_tokens);

EventEnvelope BuildMessageStop();

// Standard error envelope used by Anthropic:
// {"type": "error", "error": {"type": "...", "message": "..."}}
nlohmann::json BuildError(const std::string& message,
                          const std::string& type);

}  // namespace omni::api::anthropic
