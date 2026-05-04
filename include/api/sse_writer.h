// Server-Sent Events helpers for streaming chat completions.
//
// Two flavours:
//
//   * OpenAI: each event has only a `data:` line whose payload is a
//     `chat.completion.chunk` JSON object. The stream terminates with
//     `data: [DONE]\n\n`.
//   * Anthropic: each event has both an `event:` line (carrying the
//     event type, e.g. "message_start") and a `data:` line with the
//     event-specific JSON. The stream terminates implicitly when the
//     server closes the chunked transfer.
//
// Both variants use the cpp-httplib `DataSink` callback signature so the
// transport layer can stay completely separate from the wire format.
#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace omni::api::sse {

// Function the writer hands serialised bytes to. The HTTP transport pushes
// these into cpp-httplib's DataSink::write. Returns true on success; the
// writer will stop emitting further events on the first false return.
using SinkFn = std::function<bool(const char* data, size_t len)>;

// OpenAI variant: emit `data: <json>\n\n`.
bool EmitOpenAI(SinkFn sink, const nlohmann::json& chunk);

// OpenAI terminator: emit `data: [DONE]\n\n`.
bool EmitOpenAIDone(SinkFn sink);

// Anthropic variant: emit `event: <name>\n` + `data: <json>\n\n`.
bool EmitAnthropic(SinkFn sink, const std::string& event_name, const nlohmann::json& data);

}  // namespace omni::api::sse
