#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

// ============================================================
// DIFY OPEN-SOURCE LLM APP DEVELOPMENT PLATFORM
// ============================================================

struct DifyConfig {
    std::string server_url = "http://localhost:5001";
    int port = 5001;
    std::string api_key = "";
    bool use_local_model = true;
    std::string local_model_name = "";
    bool auto_start_server = true;
};

struct DifyApp {
    std::string app_id;
    std::string app_name;
    std::string app_type; // "text-generation", "chat", "workflow", "agent"
    json configuration;
    std::string description;
};

struct DifyConversation {
    std::string conversation_id;
    std::vector<std::pair<std::string, std::string>> messages; // (role, content)
};

struct DifyResponse {
    std::string message_id;
    std::string content;
    json metadata;
    double latency_ms = 0.0;
    std::vector<std::string> tool_calls;
};

class DifyIntegration {
public:
    DifyIntegration(const DifyConfig& config = DifyConfig());
    ~DifyIntegration();

    // Server Management
    bool StartServer();
    bool StopServer();
    bool IsServerRunning();
    bool ConnectToServer();

    // App Management
    std::vector<DifyApp> GetAvailableApps();
    bool CreateApp(const std::string& app_name, const std::string& app_type, const json& config);
    bool DeleteApp(const std::string& app_id);
    bool UpdateApp(const std::string& app_id, const json& config);

    // Text Generation
    DifyResponse GenerateText(
        const DifyApp& app,
        const std::string& prompt,
        const json& variables = json()
    );

    // Chat Interface
    DifyResponse Chat(
        const DifyApp& app,
        const std::string& message,
        const std::string& conversation_id = "",
        const json& variables = json()
    );

    // Conversation Management
    DifyConversation GetConversation(const std::string& conversation_id);
    std::vector<DifyConversation> GetAllConversations(const std::string& app_id);
    bool DeleteConversation(const std::string& conversation_id);

    // Workflow Execution
    DifyResponse ExecuteWorkflow(
        const DifyApp& app,
        const json& workflow_inputs
    );

    // Agent Execution
    DifyResponse ExecuteAgent(
        const DifyApp& app,
        const std::string& user_input,
        const json& tools = json()
    );

    // Tool Registration
    bool RegisterTool(const std::string& app_id, const json& tool_definition);

private:
    DifyConfig config_;
    CURL* curl_handle_ = nullptr;
    std::string server_process_id_;
    bool is_connected_ = false;

    std::string MakeRequest(const std::string& server_url, const std::string& endpoint, const std::string& method, const json& data = json());
    json ParseResponse(const std::string& response);
};