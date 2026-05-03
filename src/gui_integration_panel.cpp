#include "gui_integration_panel.h"
#include <iostream>

GUIIntegrationPanel::GUIIntegrationPanel() = default;

void GUIIntegrationPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(1600, 900), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("🔗 Integration Control Panel", nullptr)) {
        ImGui::BeginTabBar("##integration_tabs");

        // TAB: Tool Calling
        if (ImGui::BeginTabItem("🛠️ Tool Calling & Functions")) {
            RenderToolCallingPanel();
            ImGui::EndTabItem();
        }

        // TAB: ComfyUI
        if (ImGui::BeginTabItem("🎨 ComfyUI (Local)")) {
            RenderComfyUIPanel();
            ImGui::EndTabItem();
        }

        // TAB: Dify
        if (ImGui::BeginTabItem("⚡ Dify LLM Platform")) {
            RenderDifyPanel();
            ImGui::EndTabItem();
        }

        // TAB: RAG
        if (ImGui::BeginTabItem("📚 RAG System")) {
            RenderRAGPanel();
            ImGui::EndTabItem();
        }

        // TAB: MiniCPM
        if (ImGui::BeginTabItem("👁️ MiniCPM-O 4.5")) {
            RenderMiniCPMPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void GUIIntegrationPanel::RenderToolCallingPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ LLAMA.CPP TOOL CALLING & FUNCTION EXECUTION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::Text("📋 Registered Tools & Functions:");

    ImGui::BeginChild("##tools_list", ImVec2(0, 250), true);

    auto tools = tool_calling_.GetAllToolDefinitions();

    ImGui::Columns(4, "##tools_cols", true);
    ImGui::Text("Tool Name");
    ImGui::NextColumn();
    ImGui::Text("Type");
    ImGui::NextColumn();
    ImGui::Text("Parameters");
    ImGui::NextColumn();
    ImGui::Text("Action");
    ImGui::NextColumn();

    ImGui::Separator();

    for (const auto& tool : tools) {
        ImGui::Text("%s", tool.tool_name.c_str());
        ImGui::NextColumn();

        const char* type_str = "";
        switch (tool.tool_type) {
            case ToolType::LOCAL_FUNCTION: type_str = "Local Func"; break;
            case ToolType::COMFYUI_WORKFLOW: type_str = "ComfyUI"; break;
            case ToolType::DIFY_WORKFLOW: type_str = "Dify"; break;
            case ToolType::EXTERNAL_API: type_str = "API"; break;
            default: type_str = "Unknown";
        }

        ImGui::Text("%s", type_str);
        ImGui::NextColumn();
        ImGui::Text("%zu", tool.parameters.size());
        ImGui::NextColumn();

        if (ImGui::Button(("Test##" + tool.tool_id).c_str())) {
            std::cout << "[GUI] Testing tool: " << tool.tool_name << std::endl;
        }

        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("🔧 Function Schema (OpenAI Compatible):");

    ImGui::BeginChild("##schema", ImVec2(0, 200), true);

    auto schema = tool_calling_.GenerateFunctionSchema();
    ImGui::TextUnformatted(schema.dump(2).c_str());

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("➕ Register New Tool:");

    static char tool_name[256] = "";
    static char tool_description[512] = "";
    static int tool_type_selected = 0;

    ImGui::InputText("Tool Name##register", tool_name, sizeof(tool_name));
    ImGui::InputTextMultiline("Description##register", tool_description, sizeof(tool_description), ImVec2(-1, 100));

    const char* type_options[] = { "Local Function", "ComfyUI Workflow", "Dify Workflow", "External API" };
    ImGui::Combo("Tool Type##register", &tool_type_selected, type_options, 4);

    if (ImGui::Button("✓ REGISTER TOOL", ImVec2(-1, 50))) {
        ToolDefinition new_tool{};
        new_tool.tool_id = std::string(tool_name);
        new_tool.tool_name = std::string(tool_name);
        new_tool.description = std::string(tool_description);
        new_tool.tool_type = static_cast<ToolType>(tool_type_selected);

        tool_calling_.RegisterTool(new_tool);

        memset(tool_name, 0, sizeof(tool_name));
        memset(tool_description, 0, sizeof(tool_description));

        std::cout << "[GUI] Tool registered successfully" << std::endl;
    }
}

void GUIIntegrationPanel::RenderComfyUIPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ COMFYUI - LOCAL GENERATIVE AI WORKFLOWS");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    // Server Status
    ImGui::BeginChild("##comfyui_status", ImVec2(0, 100), true);

    bool running = comfyui_.IsServerRunning();
    RenderConnectionStatus("ComfyUI Server", running);

    if (!running) {
        if (ImGui::Button("▶ START COMFYUI SERVER", ImVec2(-1, 50))) {
            comfyui_.StartServer();
        }
    } else {
        if (ImGui::Button("⛔ STOP COMFYUI SERVER", ImVec2(-1, 50))) {
            comfyui_.StopServer();
        }
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Image Generation
    ImGui::Text("🎨 Image Generation:");

    ImGui::BeginChild("##image_gen", ImVec2(0, 300), true);

    static char prompt[1024] = "A beautiful landscape at sunset";
    static int width = 512;
    static int height = 512;
    static int steps = 20;
    static float guidance = 7.5f;

    ImGui::InputTextMultiline("Prompt##comfyui", prompt, sizeof(prompt), ImVec2(-1, 100));
    ImGui::SliderInt("Width##comfyui", &width, 256, 2048, "%d");
    ImGui::SliderInt("Height##comfyui", &height, 256, 2048, "%d");
    ImGui::SliderInt("Steps##comfyui", &steps, 1, 100);
    ImGui::SliderFloat("Guidance Scale##comfyui", &guidance, 0.0f, 20.0f);

    if (ImGui::Button("🎨 GENERATE IMAGE", ImVec2(-1, 50))) {
        std::string task_id = comfyui_.GenerateImage(prompt, width, height, steps, guidance);
        std::cout << "[ComfyUI] Task started: " << task_id << std::endl;
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Model Management
    ImGui::Text("📦 Model Management:");

    ImGui::BeginChild("##models", ImVec2(0, 150), true);

    auto models = comfyui_.GetAvailableModels();

    for (const auto& model : models) {
        ImGui::Selectable(model.c_str(), false);
    }

    ImGui::EndChild();
}

void GUIIntegrationPanel::RenderDifyPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ DIFY - OPEN-SOURCE LLM APP DEVELOPMENT");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    bool running = dify_.IsServerRunning();
    RenderConnectionStatus("Dify Server", running);

    if (!running) {
        if (ImGui::Button("▶ START DIFY SERVER", ImVec2(-1, 50))) {
            dify_.StartServer();
        }
    } else {
        if (ImGui::Button("⛔ STOP DIFY SERVER", ImVec2(-1, 50))) {
            dify_.StopServer();
        }
    }

    ImGui::Separator();

    ImGui::Text("💬 Chat Interface:");

    ImGui::BeginChild("##dify_chat", ImVec2(0, 400), true);

    static char dify_message[1024] = "";
    ImGui::InputTextMultiline("Message##dify", dify_message, sizeof(dify_message), ImVec2(-1, 100));

    if (ImGui::Button("SEND", ImVec2(-1, 50))) {
        std::cout << "[Dify] Sending message..." << std::endl;
    }

    ImGui::EndChild();
}

void GUIIntegrationPanel::RenderRAGPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ RAG SYSTEM - UltraRAG + MiniCPM-O 4.5");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::Text("📊 RAG System Status:");

    ImGui::BeginChild("##rag_status", ImVec2(0, 100), true);

    RenderConnectionStatus("UltraRAG Server", rag_.IsServerRunning());
    RenderConnectionStatus("MiniCPM Server", rag_.IsServerRunning());

    if (ImGui::Button("▶ START RAG SERVERS", ImVec2(-1, 50))) {
        rag_.StartUltraRAGServer();
        rag_.StartMiniCPMServer();
    }

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("📚 Document Ingestion:");

    ImGui::BeginChild("##rag_ingest", ImVec2(0, 150), true);

    if (ImGui::Button("📁 Add Local Directory", ImVec2(-1, 40))) {
        std::cout << "[RAG] Add directory" << std::endl;
    }

    if (ImGui::Button("📦 Add Git Repository", ImVec2(-1, 40))) {
        std::cout << "[RAG] Add Git repo" << std::endl;
    }

    if (ImGui::Button("🔗 Add GitIngest URL", ImVec2(-1, 40))) {
        std::cout << "[RAG] Add GitIngest" << std::endl;
    }

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("🔍 RAG Query:");

    ImGui::BeginChild("##rag_query", ImVec2(0, 300), true);

    static char rag_query[1024] = "What is the main topic of the documents?";
    ImGui::InputTextMultiline("Query##rag", rag_query, sizeof(rag_query), ImVec2(-1, 100));

    if (ImGui::Button("🔍 QUERY & SYNTHESIZE", ImVec2(-1, 50))) {
        RAGQuery query{};
        query.query_text = std::string(rag_query);
        query.top_k = 5;

        RAGResult result = rag_.QueryRAG(query);
        std::cout << "[RAG] Answer: " << result.synthesized_answer << std::endl;
    }

    ImGui::EndChild();
}

void GUIIntegrationPanel::RenderMiniCPMPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MINICPM-O 4.5 - MULTIMODAL UNDERSTANDING");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::Text("👁️ Multimodal Processing:");

    ImGui::BeginChild("##minicpm_processing", ImVec2(0, 300), true);

    if (ImGui::Button("📸 Process Image", ImVec2(-1, 50))) {
        std::cout << "[MiniCPM] Process image" << std::endl;
    }

    if (ImGui::Button("🎬 Process Video", ImVec2(-1, 50))) {
        std::cout << "[MiniCPM] Process video" << std::endl;
    }

    if (ImGui::Button("📄 Extract Text (OCR)", ImVec2(-1, 50))) {
        std::cout << "[MiniCPM] OCR" << std::endl;
    }

    if (ImGui::Button("🔍 Detect Objects", ImVec2(-1, 50))) {
        std::cout << "[MiniCPM] Object detection" << std::endl;
    }

    ImGui::EndChild();
}

void GUIIntegrationPanel::RenderServerStatus(const std::string& name, bool running) {
    ImVec4 color = running ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s %s", running ? "✓" : "✗", name.c_str());
    ImGui::PopStyleColor();
}

void GUIIntegrationPanel::RenderConnectionStatus(const std::string& name, bool connected) {
    ImVec4 color = connected ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, color);

    if (ImGui::Button((name + (connected ? " [CONNECTED]" : " [DISCONNECTED]")).c_str(), ImVec2(-1, 35))) {
        // Toggle connection
    }

    ImGui::PopStyleColor();
}