#pragma once

#include "omni_engine.h"
#include "gui_node_editor.h"
#include <SDL2/SDL.h>
#include <imgui.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class GUIMainWindow {
public:
    GUIMainWindow();
    ~GUIMainWindow();

    bool Initialize(int width = 1600, int height = 900);
    void Run();
    void Shutdown();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;

    OmniEngine engine_;
    GUINodeEditor node_editor_;
    ImGuiIO* io_ = nullptr;

    // Loop control + synchronisation for output produced by background threads.
    std::atomic<bool> running_{false};
    mutable std::mutex output_mutex_;

    // UI State
    bool show_hardware_info_ = false;
    bool show_model_settings_ = false;
    bool show_generation_panel_ = false;
    char input_prompt_[1024] = {};
    char output_buffer_[8192] = {};
    std::vector<std::string> available_models_;
    int selected_model_ = -1;

    // Model parameters UI
    ModelParameters ui_model_params_;
    GenerationConfig ui_gen_config_;
    TurboQuantConfig ui_quant_config_;

    // Rendering
    void Render();
    void RenderMenuBar();
    void RenderHardwareInfoWindow();
    void RenderModelSettingsWindow();
    void RenderGenerationPanel();
    void RenderNodeEditor();
    void RenderStatusBar();

    // Logic
    void ScanModels();
    void LoadSelectedModel();
    void ExecuteGeneration();
    void UpdateEngineSettings();
};