#include "tool_calling_interface.h"
#include <iostream>
#include <chrono>
#include <regex>

ToolCallingInterface::ToolCallingInterface() = default;

void ToolCallingInterface::RegisterTool(const ToolDefinition& tool_def) {
    registered_tools_[tool_def.tool_id] = tool_def;
    std::cout << "[ToolCalling] Registered tool: " << tool_def.tool_name << std::endl;
}

void ToolCallingInterface::UnregisterTool(const std::string& tool_id) {
    registered_tools_.erase(tool_id);
    tool_callbacks_.erase(tool_id);
}

ToolDefinition ToolCallingInterface::GetToolDefinition(const std::string& tool_id) const {
    auto it = registered_tools_.find(tool_id);
    if (it != registered_tools_.end()) {
        return it->second;
    }
    return ToolDefinition{};
}

std::vector<ToolDefinition> ToolCallingInterface::GetAllToolDefinitions() const {
    std::vector<ToolDefinition> tools;
    for (const auto& [id, def] : registered_tools_) {
        tools.push_back(def);
    }
    return tools;
}

FunctionCall ToolCallingInterface::ParseFunctionCall(const std::string& model_output) {
    FunctionCall call{};

    // Parse JSON function call format
    // Expected: {"tool_name": "...", "tool_id": "...", "arguments": {...}}
    try {
        size_t json_start = model_output.find('{');
        size_t json_end = model_output.rfind('}');

        if (json_start != std::string::npos && json_end != std::string::npos) {
            std::string json_str = model_output.substr(json_start, json_end - json_start + 1);
            json parsed = json::parse(json_str);

            call.tool_name = parsed.value("tool_name", "");
            call.tool_id = parsed.value("tool_id", "");
            call.arguments = parsed.value("arguments", json());
            call.call_id = parsed.value("call_id", std::to_string(std::time(nullptr)));
        }
    } catch (const std::exception& e) {
        std::cerr << "[ToolCalling] Parse error: " << e.what() << std::endl;
    }

    return call;
}

std::vector<FunctionCall> ToolCallingInterface::ParseMultipleFunctionCalls(const std::string& model_output) {
    std::vector<FunctionCall> calls;

    // Find all JSON objects in the output
    std::regex json_regex(R"(\{[^{}]*\})");
    std::sregex_iterator begin(model_output.begin(), model_output.end(), json_regex);
    std::sregex_iterator end;

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string json_str = i->str();
        try {
            json parsed = json::parse(json_str);
            if (parsed.contains("tool_name") && parsed.contains("arguments")) {
                FunctionCall call{};
                call.tool_name = parsed.value("tool_name", "");
                call.tool_id = parsed.value("tool_id", "");
                call.arguments = parsed.value("arguments", json());
                call.call_id = parsed.value("call_id", std::to_string(std::time(nullptr)));
                calls.push_back(call);
            }
        } catch (...) {
            // Skip invalid JSON
        }
    }

    return calls;
}

ToolExecutionResult ToolCallingInterface::ExecuteTool(const FunctionCall& call) {
    auto start_time = std::chrono::high_resolution_clock::now();

    ToolExecutionResult result{};
    result.tool_id = call.tool_id;
    result.call_id = call.call_id;
    result.success = false;

    auto tool_it = registered_tools_.find(call.tool_id);
    if (tool_it == registered_tools_.end()) {
        result.error_message = "Tool not found: " + call.tool_id;
        return result;
    }

    const auto& tool_def = tool_it->second;

    try {
        switch (tool_def.tool_type) {
            case ToolType::LOCAL_FUNCTION:
                result.result = ExecuteLocalFunction(call);
                result.success = true;
                break;

            case ToolType::COMFYUI_WORKFLOW:
                result.result = ExecuteComfyUIWorkflow(call);
                result.success = true;
                break;

            case ToolType::DIFY_WORKFLOW:
                result.result = ExecuteDifyWorkflow(call);
                result.success = true;
                break;

            case ToolType::EXTERNAL_API:
                result.result = ExecuteExternalAPI(call);
                result.success = true;
                break;

            case ToolType::FILE_SYSTEM:
                result.result = ExecuteFileSystemOperation(call);
                result.success = true;
                break;

            default:
                result.error_message = "Unsupported tool type";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "[ToolCalling] Executed " << call.tool_name << " in " << result.execution_time_ms << "ms" << std::endl;

    return result;
}

std::vector<ToolExecutionResult> ToolCallingInterface::ExecuteToolSequence(const std::vector<FunctionCall>& calls) {
    std::vector<ToolExecutionResult> results;
    for (const auto& call : calls) {
        results.push_back(ExecuteTool(call));
    }
    return results;
}

std::string ToolCallingInterface::FormatToolResultForModel(const ToolExecutionResult& result) {
    json formatted{};
    formatted["call_id"] = result.call_id;
    formatted["success"] = result.success;
    formatted["result"] = result.result;
    if (!result.error_message.empty()) {
        formatted["error"] = result.error_message;
    }
    return formatted.dump();
}

std::string ToolCallingInterface::FormatMultipleResultsForModel(const std::vector<ToolExecutionResult>& results) {
    json formatted = json::array();
    for (const auto& result : results) {
        formatted.push_back(json::parse(FormatToolResultForModel(result)));
    }
    return formatted.dump();
}

json ToolCallingInterface::GenerateFunctionSchema() const {
    json schema = json::array();
    for (const auto& [id, tool] : registered_tools_) {
        schema.push_back(GenerateToolSchema(id));
    }
    return schema;
}

json ToolCallingInterface::GenerateToolSchema(const std::string& tool_id) const {
    auto it = registered_tools_.find(tool_id);
    if (it == registered_tools_.end()) {
        return json();
    }

    const auto& tool = it->second;

    json schema{};
    schema["type"] = "function";
    schema["function"]["name"] = tool.tool_name;
    schema["function"]["description"] = tool.description;

    json parameters{};
    parameters["type"] = "object";

    json properties = json::object();
    std::vector<std::string> required_params;

    for (const auto& param : tool.parameters) {
        json param_schema{};
        param_schema["type"] = param.type;
        param_schema["description"] = param.description;

        if (!param.enum_values.empty()) {
            param_schema["enum"] = param.enum_values;
        }

        if (!param.default_value.is_null()) {
            param_schema["default"] = param.default_value;
        }

        properties[param.name] = param_schema;

        if (param.required) {
            required_params.push_back(param.name);
        }
    }

    parameters["properties"] = properties;
    parameters["required"] = required_params;

    schema["function"]["parameters"] = parameters;

    return schema;
}

void ToolCallingInterface::RegisterToolCallback(const std::string& tool_id, ToolCallback callback) {
    tool_callbacks_[tool_id] = callback;
}

json ToolCallingInterface::ExecuteLocalFunction(const FunctionCall& call) {
    auto callback_it = tool_callbacks_.find(call.tool_id);
    if (callback_it != tool_callbacks_.end()) {
        return callback_it->second(call.arguments);
    }
    return json{{"error", "No callback registered for tool"}};
}

json ToolCallingInterface::ExecuteComfyUIWorkflow(const FunctionCall& call) {
    // Implemented in comfyui_integration.cpp
    return json{{"status", "pending"}};
}

json ToolCallingInterface::ExecuteDifyWorkflow(const FunctionCall& call) {
    // Implemented in dify_integration.cpp
    return json{{"status", "pending"}};
}

json ToolCallingInterface::ExecuteExternalAPI(const FunctionCall& call) {
    // Make HTTP request to external API
    return json{{"status", "pending"}};
}

json ToolCallingInterface::ExecuteFileSystemOperation(const FunctionCall& call) {
    // Execute file system operations safely
    return json{{"status", "pending"}};
}