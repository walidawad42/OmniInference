// Translation between OpenAI's `tools[]` / `tool_calls[]` schema and
// Anthropic's `tool_use` / `tool_result` content blocks.
//
// Both schemas describe the same conceptual model (LLM emits a structured
// tool call, host runs the tool, host feeds the result back), they just
// disagree on where the data lives in JSON. The translator lets a single
// ToolDefinition / ToolCall set drive both endpoints.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "api/chat_request.h"

namespace omni::api {

namespace openai_tools {

// Parse an OpenAI `tools` array into our internal representation.
//
// Each entry is {"type": "function", "function": {"name", "description",
// "parameters"}}.
std::vector<ToolDefinition> ParseTools(const nlohmann::json& tools_array);

// Inverse: serialise our tool definitions as the OpenAI `tools` array.
nlohmann::json SerializeTools(const std::vector<ToolDefinition>& tools);

// Parse an OpenAI `tool_calls` array (as found inside an assistant message
// from a multi-turn conversation) into our ToolCall list.
std::vector<ToolCall> ParseToolCalls(const nlohmann::json& tool_calls_array);

// Inverse: serialise our tool calls as the OpenAI `tool_calls` array.
nlohmann::json SerializeToolCalls(const std::vector<ToolCall>& tool_calls);

}  // namespace openai_tools

namespace anthropic_tools {

// Parse an Anthropic `tools` array into our internal representation.
// Each entry is {"name", "description", "input_schema"}.
std::vector<ToolDefinition> ParseTools(const nlohmann::json& tools_array);

// Inverse: serialise our tool definitions as the Anthropic `tools` array.
nlohmann::json SerializeTools(const std::vector<ToolDefinition>& tools);

// Build the assistant message content for a non-streaming response that
// includes tool calls. Anthropic interleaves text and tool_use blocks in a
// single content array; this helper produces that array.
nlohmann::json BuildAssistantContentBlocks(const std::string& text,
                                           const std::vector<ToolCall>& tool_calls);

}  // namespace anthropic_tools

}  // namespace omni::api
