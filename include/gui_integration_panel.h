#pragma once

#include "tool_calling_interface.h"
#include "comfyui_integration.h"
#include "dify_integration.h"
#include "rag_integration.h"
#include "minicpm_integration.h"
#include "gui_imgui_compat.h"
#include <string>
#include <vector>

class GUIIntegrationPanel {
public:
    GUIIntegrationPanel();
    ~GUIIntegrationPanel() = default;

    void Render();

private:
    // Integration instances
    ToolCallingInterface tool_calling_;
    ComfyUIIntegration comfyui_;
    DifyIntegration dify_;
    RAGIntegration rag_;
    MiniCPMIntegration minicpm_;

    // UI State
    bool show_tool_calling_ = false;
    bool show_comfyui_ = false;
    bool show_dify_ = false;
    bool show_rag_ = false;
    bool show_minicpm_ = false;

    // Rendering methods
    void RenderToolCallingPanel();
    void RenderComfyUIPanel();
    void RenderDifyPanel();
    void RenderRAGPanel();
    void RenderMiniCPMPanel();

    // Common UI elements
    void RenderServerStatus(const std::string& name, bool running);
    void RenderConnectionStatus(const std::string& name, bool connected);
};