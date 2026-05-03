#pragma once

#include "omni_engine.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <memory>

enum class GUIState {
    IDLE,
    LOADING_MODEL,
    GENERATING,
    ERROR
};

struct ModelEntry {
    std::string name;
    std::string path;
    std::string type; // "LLM" or "MLLM"
    int size_mb;
    bool loaded;
};

class GUISettings {
public:
    int n_ctx = 4096;
    int n_batch = 512;
    int n_gpu_layers = 100;
    float temperature = 0.7f;
    float top_p = 0.95f;
    int max_tokens = 512;
    bool use_gpu = true;
    std::string model_directory = "./models";
    
    void SaveToFile(const std::string& filepath);
    void LoadFromFile(const std::string& filepath);
};

class GUIApp {
public:
    GUIApp();
    ~GUIApp();
    
    bool Initialize(int width = 1280, int height = 720);
    void Run();
    void Shutdown();
    
private:
    // Window & Rendering
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int window_width_ = 1280;
    int window_height_ = 720;
    
    // State
    GUIState state_ = GUIState::IDLE;
    OmniEngine engine_;
    GUISettings settings_;
    std::vector<ModelEntry> available_models_;
    ModelEntry* current_model_ = nullptr;
    
    // GUI Components State
    std::string input_prompt_;
    std::string output_text_;
    std::string status_message_;
    bool show_settings_panel_ = false;
    bool show_model_browser_ = false;
    
    // Event Handling
    void HandleEvents();
    void HandleInput(const SDL_Event& event);
    
    // Rendering
    void Render();
    void RenderMainPanel();
    void RenderTopBar();
    void RenderPromptInput();
    void RenderOutputDisplay();
    void RenderStatusBar();
    void RenderSettingsPanel();
    void RenderModelBrowser();
    void RenderSidebar();
    
    // Model Management
    void ScanModelDirectory();
    void LoadModel(const std::string& model_path);
    void UnloadModel();
    void RefreshModelList();
    
    // Helper Functions
    void DrawText(int x, int y, const std::string& text, SDL_Color color);
    void DrawRectangle(int x, int y, int w, int h, SDL_Color color, bool filled = true);
    void DrawButton(int x, int y, int w, int h, const std::string& label, bool hovered);
    
    bool IsButtonHovered(int x, int y, int w, int h, int mx, int my);
    
    // Generation
    void GenerateResponse();
    void OnTokenGenerated(const std::string& token);
};