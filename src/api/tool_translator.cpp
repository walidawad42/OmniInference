#include "api/tool_translator.h"

#include <stdexcept>

namespace omni::api {

// ============================================================
// OpenAI tools <-> internal
// ============================================================
namespace openai_tools {

std::vector<ToolDefinition> ParseTools(const nlohmann::json& tools_array) {
    std::vector<ToolDefinition> out;
    if (!tools_array.is_array()) {
        return out;
    }
    out.reserve(tools_array.size());
    for (const auto& entry : tools_array) {
        // Accept both the new {"type":"function","function":{...}} shape and
        // the legacy {"name":...,"description":...,"parameters":...} shape.
        const nlohmann::json* fn = nullptr;
        if (entry.contains("function") && entry["function"].is_object()) {
            fn = &entry["function"];
        } else if (entry.contains("name")) {
            fn = &entry;
        } else {
            continue;
        }
        ToolDefinition def;
        def.name        = fn->value("name", "");
        def.description = fn->value("description", "");
        if (fn->contains("parameters")) {
            def.parameters_schema = (*fn)["parameters"];
        } else if (fn->contains("input_schema")) {
            // Legacy alias — some clients send Anthropic's name even on the
            // OpenAI endpoint. Accept it.
            def.parameters_schema = (*fn)["input_schema"];
        } else {
            def.parameters_schema = nlohmann::json::object();
        }
        if (def.name.empty()) {
            continue;
        }
        out.push_back(std::move(def));
    }
    return out;
}

nlohmann::json SerializeTools(const std::vector<ToolDefinition>& tools) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& t : tools) {
        nlohmann::json fn = {
            {"name",        t.name},
            {"description", t.description},
            {"parameters",  t.parameters_schema},
        };
        out.push_back({{"type", "function"}, {"function", std::move(fn)}});
    }
    return out;
}

std::vector<ToolCall> ParseToolCalls(const nlohmann::json& tool_calls_array) {
    std::vector<ToolCall> out;
    if (!tool_calls_array.is_array()) {
        return out;
    }
    out.reserve(tool_calls_array.size());
    for (const auto& tc : tool_calls_array) {
        ToolCall call;
        call.id = tc.value("id", "");
        if (tc.contains("function") && tc["function"].is_object()) {
            const auto& fn = tc["function"];
            call.name = fn.value("name", "");
            // OpenAI sends `arguments` as a *string* containing JSON.
            // Parse it lazily; on parse failure keep the raw string.
            const auto& args = fn["arguments"];
            if (args.is_string()) {
                try {
                    call.arguments = nlohmann::json::parse(args.get<std::string>());
                } catch (...) {
                    call.arguments = args.get<std::string>();
                }
            } else {
                call.arguments = args;
            }
        }
        if (!call.name.empty()) {
            out.push_back(std::move(call));
        }
    }
    return out;
}

nlohmann::json SerializeToolCalls(const std::vector<ToolCall>& tool_calls) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& call : tool_calls) {
        out.push_back({
            {"id",       call.id},
            {"type",     "function"},
            {"function", {
                {"name",      call.name},
                // OpenAI requires `arguments` to be a string; serialise the
                // JSON value back out so downstream parsers see the canonical
                // representation.
                {"arguments", call.arguments.is_string()
                                  ? call.arguments.get<std::string>()
                                  : call.arguments.dump()},
            }},
        });
    }
    return out;
}

}  // namespace openai_tools

// ============================================================
// Anthropic tools <-> internal
// ============================================================
namespace anthropic_tools {

std::vector<ToolDefinition> ParseTools(const nlohmann::json& tools_array) {
    std::vector<ToolDefinition> out;
    if (!tools_array.is_array()) {
        return out;
    }
    out.reserve(tools_array.size());
    for (const auto& entry : tools_array) {
        ToolDefinition def;
        def.name        = entry.value("name", "");
        def.description = entry.value("description", "");
        if (entry.contains("input_schema")) {
            def.parameters_schema = entry["input_schema"];
        } else if (entry.contains("parameters")) {
            // Tolerate OpenAI-style key.
            def.parameters_schema = entry["parameters"];
        } else {
            def.parameters_schema = nlohmann::json::object();
        }
        if (def.name.empty()) {
            continue;
        }
        out.push_back(std::move(def));
    }
    return out;
}

nlohmann::json SerializeTools(const std::vector<ToolDefinition>& tools) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& t : tools) {
        out.push_back({
            {"name",         t.name},
            {"description",  t.description},
            {"input_schema", t.parameters_schema},
        });
    }
    return out;
}

nlohmann::json BuildAssistantContentBlocks(const std::string& text,
                                           const std::vector<ToolCall>& tool_calls) {
    nlohmann::json blocks = nlohmann::json::array();
    if (!text.empty()) {
        blocks.push_back({
            {"type", "text"},
            {"text", text},
        });
    }
    for (const auto& call : tool_calls) {
        // Anthropic always wants `input` to be an object. If the model emitted
        // a string or array, wrap it under a stable key so downstream tools
        // still get something structured.
        nlohmann::json input = call.arguments;
        if (!input.is_object()) {
            input = nlohmann::json{{"value", call.arguments}};
        }
        blocks.push_back({
            {"type",  "tool_use"},
            {"id",    call.id},
            {"name",  call.name},
            {"input", input},
        });
    }
    return blocks;
}

}  // namespace anthropic_tools

}  // namespace omni::api
