#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <json.hpp>

using json = nlohmann::json;

// ============================================================
// TOOL CALLING SYSTEM - llama.cpp Function Calling
// ============================================================

enum class ToolType {
    EXTERNAL_API,
    LOCAL_FUNCTION,
    COMFYUI_WORKFLOW,
    DIFY_WORKFLOW,
    CODE_EXECUTION,
    FILE_SYSTEM,
    DATABASE
};

struct FunctionParameter {
    std::string name;
    std::string type; // "string", "number", "boolean", "array", "object"
    std::string description;
    bool required = true;
    json default_value;
    std::vector<std::string> enum_values;
};

struct ToolDefinition {
    std::string tool_id;
    std::string tool_name;
    std::string description;
    ToolType tool_type;
    std::vector<FunctionParameter> parameters;
    std::string endpoint_url; // For API tools
    std::string function_signature; // For local functions
    json metadata;
};

struct FunctionCall {
    std::string tool_id;
    std::string tool_name;
    json arguments;
    std::string call_id;
};

struct ToolExecutionResult {
    std::string tool_id;
    std::string call_id;
    bool success;
    json result;
    std::string error_message;
    double execution_time_ms;
};

class ToolCallingInterface {
public:
    ToolCallingInterface();
    ~ToolCallingInterface() = default;

    // Tool Registration
    void RegisterTool(const ToolDefinition& tool_def);
    void UnregisterTool(const std::string& tool_id);
    ToolDefinition GetToolDefinition(const std::string& tool_id) const;
    std::vector<ToolDefinition> GetAllToolDefinitions() const;

    // Function Calling
    FunctionCall ParseFunctionCall(const std::string& model_output);
    std::vector<FunctionCall> ParseMultipleFunctionCalls(const std::string& model_output);

    // Tool Execution
    ToolExecutionResult ExecuteTool(const FunctionCall& call);
    std::vector<ToolExecutionResult> ExecuteToolSequence(const std::vector<FunctionCall>& calls);

    // Tool Result Formatting for Model
    std::string FormatToolResultForModel(const ToolExecutionResult& result);
    std::string FormatMultipleResultsForModel(const std::vector<ToolExecutionResult>& results);

    // OpenAI-compatible function schema
    json GenerateFunctionSchema() const;
    json GenerateToolSchema(const std::string& tool_id) const;

    // Tool callbacks
    using ToolCallback = std::function<json(const json& args)>;
    void RegisterToolCallback(const std::string& tool_id, ToolCallback callback);

private:
    std::map<std::string, ToolDefinition> registered_tools_;
    std::map<std::string, ToolCallback> tool_callbacks_;
    std::map<std::string, FunctionCall> active_calls_;

    json ExecuteLocalFunction(const FunctionCall& call);
    json ExecuteComfyUIWorkflow(const FunctionCall& call);
    json ExecuteDifyWorkflow(const FunctionCall& call);
    json ExecuteExternalAPI(const FunctionCall& call);
    json ExecuteFileSystemOperation(const FunctionCall& call);
};