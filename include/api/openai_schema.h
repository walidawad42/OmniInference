// Wire-format <-> internal translation for the OpenAI Chat Completions API.
//
// Reference: https://platform.openai.com/docs/api-reference/chat/create
//
// The shape we accept is intentionally permissive (treats unknown fields as
// no-ops, accepts both `tools` and the deprecated `functions` field, accepts
// `content` as either a plain string or an array of content parts).
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "api/chat_request.h"

namespace omni::api::openai {

// Parse an OpenAI `/v1/chat/completions` POST body into our internal
// ChatRequest. Throws std::invalid_argument on malformed input.
ChatRequest ParseChatRequest(const nlohmann::json& body);

// Serialise a non-streaming ChatResponse as OpenAI's
// `chat.completion` object.
nlohmann::json SerializeChatResponse(const ChatResponse& response);

// Serialise a single SSE chunk as the `chat.completion.chunk` JSON object
// that goes after `data: ` on the wire. The caller (sse_writer.cpp) handles
// the SSE framing.
nlohmann::json SerializeStreamChunk(const StreamChunk& chunk,
                                    const std::string& response_id,
                                    const std::string& model);

// Parse an OpenAI `/v1/completions` (legacy) POST body.
struct LegacyCompletionRequest {
    std::string          model;
    std::string          prompt;
    GenerationParameters params;
};
LegacyCompletionRequest ParseLegacyCompletionRequest(const nlohmann::json& body);
nlohmann::json SerializeLegacyCompletionResponse(const LegacyCompletionRequest& req,
                                                 const ChatResponse& response);

// Embeddings request: { "input": "..." | ["...", ...], "model": "..." }
struct EmbeddingsRequest {
    std::string                model;
    std::vector<std::string>   inputs;  // always normalised to a list
};
EmbeddingsRequest ParseEmbeddingsRequest(const nlohmann::json& body);

// Build the standard 501-style error body used when no embedding model is
// loaded. Returns the JSON object; the HTTP layer is responsible for the
// status code.
nlohmann::json BuildEmbeddingsNotImplemented(const std::string& detail);

// /v1/models response body.
nlohmann::json SerializeModelsList(const std::vector<std::string>& model_ids);

// Standard error envelope: {"error": {"message": ..., "type": ..., "code": ...}}
nlohmann::json BuildError(const std::string& message,
                          const std::string& type,
                          const std::string& code);

}  // namespace omni::api::openai
