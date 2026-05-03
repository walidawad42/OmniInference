#include "gui_app.h"
#include <iostream>
#include <filesystem>
#include <thread>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

GUIApp::GUIApp() = default;

GUIApp::~GUIApp() {
    Shutdown();
}

bool GUIApp::Initialize(int width, int height) {
    window_width_ = width;
    window_height_ = height;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL Initialization Failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create Window
    window_ = SDL_CreateWindow(
        "OmniInference - LLM/MLLM GUI",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window_) {
        std::cerr << "Window Creation Failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    
    // Create Renderer
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "Renderer Creation Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }
    
    // Initialize Hardware
    if (!engine_.InitializeHardware()) {
        std::cerr << "Hardware Initialization Failed" << std::endl;
        return false;
    }
    
    // Load Settings
    settings_.LoadFromFile("omni_settings.cfg");
    
    // Scan Available Models
    ScanModelDirectory();
    
    status_message_ = "Ready. Select a model to begin.";
    
    return true;
}

void GUIApp::Run() {
    bool running = true;
    
    while (running) {
        HandleEvents();
        Render();
        SDL_Delay(16); // ~60 FPS
    }
}

void GUIApp::Shutdown() {
    UnloadModel();
    settings_.SaveToFile("omni_settings.cfg");
    
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void GUIApp::HandleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
                HandleInput(event);
                break;
        }
    }
}

void GUIApp::HandleInput(const SDL_Event& event) {
    static int mouse_x = 0, mouse_y = 0;
    
    if (event.type == SDL_MOUSEMOTION) {
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
    }
    
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        // Settings Button (Top Right)
        if (IsButtonHovered(window_width_ - 120, 10, 100, 40, mouse_x, mouse_y)) {
            show_settings_panel_ = !show_settings_panel_;
        }
        
        // Model Browser Button
        if (IsButtonHovered(window_width_ - 240, 10, 100, 40, mouse_x, mouse_y)) {
            show_model_browser_ = !show_model_browser_;
        }
    }
    
    if (event.type == SDL_TEXTINPUT) {
        if (!show_settings_panel_ && !show_model_browser_) {
            input_prompt_ += event.text.text;
        }
    }
    
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_BACKSPACE && !input_prompt_.empty()) {
            input_prompt_.pop_back();
        }
        
        if (event.key.keysym.sym == SDLK_RETURN && current_model_ && current_model_->loaded) {
            GenerateResponse();
        }
    }
}

void GUIApp::Render() {
    // Clear Screen
    SDL_SetRenderDrawColor(renderer_, 30, 30, 40, 255);
    SDL_RenderClear(renderer_);
    
    // Render Main UI
    RenderTopBar();
    RenderSidebar();
    RenderPromptInput();
    RenderOutputDisplay();
    RenderStatusBar();
    
    // Render Overlay Panels
    if (show_settings_panel_) {
        RenderSettingsPanel();
    }
    if (show_model_browser_) {
        RenderModelBrowser();
    }
    
    SDL_RenderPresent(renderer_);
}

void GUIApp::RenderTopBar() {
    // Top Navigation Bar
    SDL_SetRenderDrawColor(renderer_, 45, 45, 60, 255);
    SDL_Rect top_bar = {0, 0, window_width_, 60};
    SDL_RenderFillRect(renderer_, &top_bar);
    
    // Title
    DrawText(15, 15, "OmniInference", {255, 255, 255, 255});
    
    // Model Status
    std::string model_status = current_model_ 
        ? current_model_->name + (current_model_->loaded ? " [LOADED]" : " [Ready]")
        : "[No Model]";
    DrawText(300, 15, model_status, current_model_ && current_model_->loaded ? SDL_Color{0, 255, 0, 255} : SDL_Color{255, 200, 0, 255});
    
    // Button: Model Browser
    DrawButton(window_width_ - 240, 10, 100, 40, "Models", false);
    
    // Button: Settings
    DrawButton(window_width_ - 120, 10, 100, 40, "Settings", false);
}

void GUIApp::RenderPromptInput() {
    int y_start = 80;
    int input_height = 150;
    
    // Label
    DrawText(15, y_start, "Prompt:", {200, 200, 200, 255});
    
    // Input Box
    SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 255);
    SDL_Rect input_box = {15, y_start + 25, window_width_ - 30, input_height};
    SDL_RenderFillRect(renderer_, &input_box);
    
    // Border
    SDL_SetRenderDrawColor(renderer_, 100, 100, 150, 255);
    SDL_RenderDrawRect(renderer_, &input_box);
    
    // Draw Text (simplified - in production use SDL_ttf)
    DrawText(25, y_start + 35, input_prompt_, {220, 220, 220, 255});
    
    // Generate Button
    bool button_hovered = false;
    DrawButton(15, y_start + input_height + 10, 150, 40, "Generate", button_hovered);
}

void GUIApp::RenderOutputDisplay() {
    int y_start = 80 + 150 + 60;
    int output_height = window_height_ - y_start - 80;
    
    // Label
    DrawText(15, y_start, "Output:", {200, 200, 200, 255});
    
    // Output Box
    SDL_SetRenderDrawColor(renderer_, 50, 50, 70, 255);
    SDL_Rect output_box = {15, y_start + 25, window_width_ - 30, output_height};
    SDL_RenderFillRect(renderer_, &output_box);
    
    // Border
    SDL_SetRenderDrawColor(renderer_, 100, 100, 150, 255);
    SDL_RenderDrawRect(renderer_, &output_box);
    
    // Draw Output Text
    DrawText(25, y_start + 35, output_text_, {200, 255, 200, 255});
}

void GUIApp::RenderStatusBar() {
    int y_pos = window_height_ - 40;
    
    SDL_SetRenderDrawColor(renderer_, 40, 40, 55, 255);
    SDL_Rect status_bar = {0, y_pos, window_width_, 40};
    SDL_RenderFillRect(renderer_, &status_bar);
    
    // Status Message
    SDL_Color status_color = {150, 200, 150, 255};
    if (state_ == GUIState::ERROR) {
        status_color = {255, 100, 100, 255};
    }
    
    DrawText(15, y_pos + 10, status_message_, status_color);
}

void GUIApp::RenderSettingsPanel() {
    // Semi-transparent overlay
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 150);
    SDL_Rect overlay = {0, 0, window_width_, window_height_};
    SDL_RenderFillRect(renderer_, &overlay);
    
    // Settings Panel
    int panel_width = 400;
    int panel_height = 500;
    int panel_x = (window_width_ - panel_width) / 2;
    int panel_y = (window_height_ - panel_height) / 2;
    
    SDL_SetRenderDrawColor(renderer_, 45, 45, 60, 255);
    SDL_Rect panel = {panel_x, panel_y, panel_width, panel_height};
    SDL_RenderFillRect(renderer_, &panel);
    
    // Border
    SDL_SetRenderDrawColor(renderer_, 150, 150, 200, 255);
    SDL_RenderDrawRect(renderer_, &panel);
    
    // Title
    DrawText(panel_x + 15, panel_y + 15, "Settings", {255, 255, 255, 255});
    
    int y_offset = panel_y + 60;
    int label_x = panel_x + 15;
    
    // Settings Display (simplified)
    std::stringstream ss;
    ss << "Context Length: " << settings_.n_ctx;
    DrawText(label_x, y_offset, ss.str(), {200, 200, 200, 255});
    
    y_offset += 40;
    ss.str("");
    ss << "Batch Size: " << settings_.n_batch;
    DrawText(label_x, y_offset, ss.str(), {200, 200, 200, 255});
    
    y_offset += 40;
    ss.str("");
    ss << "Temperature: " << settings_.temperature;
    DrawText(label_x, y_offset, ss.str(), {200, 200, 200, 255});
    
    y_offset += 40;
    ss.str("");
    ss << "Max Tokens: " << settings_.max_tokens;
    DrawText(label_x, y_offset, ss.str(), {200, 200, 200, 255});
    
    y_offset += 40;
    DrawText(label_x, y_offset, settings_.use_gpu ? "GPU: ON" : "GPU: OFF", 
             settings_.use_gpu ? SDL_Color{0, 255, 0, 255} : SDL_Color{255, 100, 100, 255});
}

void GUIApp::RenderModelBrowser() {
    // Semi-transparent overlay
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 150);
    SDL_Rect overlay = {0, 0, window_width_, window_height_};
    SDL_RenderFillRect(renderer_, &overlay);
    
    // Model Browser Panel
    int panel_width = 500;
    int panel_height = 600;
    int panel_x = (window_width_ - panel_width) / 2;
    int panel_y = (window_height_ - panel_height) / 2;
    
    SDL_SetRenderDrawColor(renderer_, 45, 45, 60, 255);
    SDL_Rect panel = {panel_x, panel_y, panel_width, panel_height};
    SDL_RenderFillRect(renderer_, &panel);
    
    // Border
    SDL_SetRenderDrawColor(renderer_, 150, 150, 200, 255);
    SDL_RenderDrawRect(renderer_, &panel);
    
    // Title
    DrawText(panel_x + 15, panel_y + 15, "Available Models", {255, 255, 255, 255});
    
    int y_offset = panel_y + 60;
    int model_height = 50;
    
    for (size_t i = 0; i < available_models_.size() && i < 10; ++i) {
        const auto& model = available_models_[i];
        
        // Model Item Background
        SDL_SetRenderDrawColor(renderer_, model.loaded ? 80, 100, 80 : 60, 60, 80, 255);
        SDL_Rect item_rect = {panel_x + 10, y_offset, panel_width - 20, model_height};
        SDL_RenderFillRect(renderer_, &item_rect);
        
        // Model Info
        std::string info = model.name + " (" + model.type + ") " + std::to_string(model.size_mb) + "MB";
        DrawText(panel_x + 20, y_offset + 10, info, 
                 model.loaded ? SDL_Color{0, 255, 0, 255} : SDL_Color{200, 200, 200, 255});
        
        y_offset += model_height + 5;
    }
}

void GUIApp::RenderSidebar() {
    // Left Sidebar
    SDL_SetRenderDrawColor(renderer_, 40, 40, 55, 255);
    SDL_Rect sidebar = {0, 60, 200, window_height_ - 100};
    SDL_RenderFillRect(renderer_, &sidebar);
}

void GUIApp::ScanModelDirectory() {
    available_models_.clear();
    
    try {
        for (const auto& entry : fs::directory_iterator(settings_.model_directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                // Check for common model formats
                if (filename.find(".gguf") != std::string::npos || 
                    filename.find(".bin") != std::string::npos) {
                    
                    ModelEntry model;
                    model.name = filename;
                    model.path = entry.path().string();
                    model.type = filename.find("vision") != std::string::npos ? "MLLM" : "LLM";
                    model.size_mb = entry.file_size() / (1024 * 1024);
                    model.loaded = false;
                    
                    available_models_.push_back(model);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning models: " << e.what() << std::endl;
    }
}

void GUIApp::LoadModel(const std::string& model_path) {
    state_ = GUIState::LOADING_MODEL;
    status_message_ = "Loading model...";
    
    TurboQuantConfig quant_cfg;
    quant_cfg.use_polar_transform = true;
    quant_cfg.key_bits = 4;
    quant_cfg.value_bits = 2;
    
    if (engine_.LoadModel(model_path, quant_cfg)) {
        state_ = GUIState::IDLE;
        status_message_ = "Model loaded successfully.";
        
        // Find and mark as loaded
        for (auto& model : available_models_) {
            if (model.path == model_path) {
                model.loaded = true;
                current_model_ = &model;
                break;
            }
        }
    } else {
        state_ = GUIState::ERROR;
        status_message_ = "Failed to load model.";
    }
}

void GUIApp::UnloadModel() {
    if (current_model_) {
        current_model_->loaded = false;
        current_model_ = nullptr;
    }
    engine_.UnloadModel();
    status_message_ = "Model unloaded.";
}

void GUIApp::GenerateResponse() {
    if (!current_model_ || !current_model_->loaded) {
        status_message_ = "No model loaded.";
        return;
    }
    
    state_ = GUIState::GENERATING;
    status_message_ = "Generating response...";
    output_text_.clear();
    
    // Run generation in background thread
    std::thread gen_thread([this]() {
        output_text_ = engine_.Generate(
            input_prompt_,
            settings_.temperature,
            settings_.top_p,
            settings_.max_tokens,
            [this](const std::string& token) {
                OnTokenGenerated(token);
            }
        );
        state_ = GUIState::IDLE;
        status_message_ = "Generation complete.";
    });
    
    gen_thread.detach();
}

void GUIApp::OnTokenGenerated(const std::string& token) {
    output_text_ += token;
}

void GUIApp::DrawText(int x, int y, const std::string& text, SDL_Color color) {
    // Simplified text rendering - in production use SDL_ttf
    // This is a placeholder; implement with actual font rendering
    (void)x; (void)y; (void)text; (void)color;
}

void GUIApp::DrawRectangle(int x, int y, int w, int h, SDL_Color color, bool filled) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    
    if (filled) {
        SDL_RenderFillRect(renderer_, &rect);
    } else {
        SDL_RenderDrawRect(renderer_, &rect);
    }
}

void GUIApp::DrawButton(int x, int y, int w, int h, const std::string& label, bool hovered) {
    SDL_Color bg_color = hovered ? SDL_Color{100, 100, 150, 255} : SDL_Color{70, 70, 100, 255};
    DrawRectangle(x, y, w, h, bg_color, true);
    DrawRectangle(x, y, w, h, {150, 150, 200, 255}, false);
    DrawText(x + 10, y + 10, label, {255, 255, 255, 255});
}

bool GUIApp::IsButtonHovered(int x, int y, int w, int h, int mx, int my) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

// Settings Serialization
void GUISettings::SaveToFile(const std::string& filepath) {
    std::ofstream file(filepath);
    file << "n_ctx=" << n_ctx << "\n";
    file << "n_batch=" << n_batch << "\n";
    file << "n_gpu_layers=" << n_gpu_layers << "\n";
    file << "temperature=" << temperature << "\n";
    file << "top_p=" << top_p << "\n";
    file << "max_tokens=" << max_tokens << "\n";
    file << "use_gpu=" << (use_gpu ? "1" : "0") << "\n";
    file << "model_directory=" << model_directory << "\n";
}

void GUISettings::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("n_ctx=") == 0) n_ctx = std::stoi(line.substr(6));
        if (line.find("n_batch=") == 0) n_batch = std::stoi(line.substr(8));
        if (line.find("n_gpu_layers=") == 0) n_gpu_layers = std::stoi(line.substr(13));
        if (line.find("temperature=") == 0) temperature = std::stof(line.substr(12));
        if (line.find("top_p=") == 0) top_p = std::stof(line.substr(6));
        if (line.find("max_tokens=") == 0) max_tokens = std::stoi(line.substr(11));
        if (line.find("use_gpu=") == 0) use_gpu = std::stoi(line.substr(8)) == 1;
        if (line.find("model_directory=") == 0) model_directory = line.substr(16);
    }
}