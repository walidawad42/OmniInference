#include "gui_app.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace {
constexpr int kBackgroundQuantKeyBits = 4;
constexpr int kBackgroundQuantValueBits = 2;
} // namespace

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
    
    // Initialize backend (uses the engine's HardwareBackend detection)
    if (!engine_.InitializeBackend()) {
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
    running_.store(true);

    while (running_.load()) {
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
                running_.store(false);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
                HandleInput(event);
                break;
            default:
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
        
        if (event.key.keysym.sym == SDLK_RETURN && current_model_index_ >= 0 &&
            available_models_[current_model_index_].loaded) {
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
    const ModelEntry* current_model = (current_model_index_ >= 0 &&
                                       current_model_index_ < static_cast<int>(available_models_.size()))
                                          ? &available_models_[current_model_index_]
                                          : nullptr;
    std::string model_status = current_model
        ? current_model->name + (current_model->loaded ? " [LOADED]" : " [Ready]")
        : "[No Model]";
    DrawText(300, 15, model_status, current_model && current_model->loaded
                                        ? SDL_Color{0, 255, 0, 255}
                                        : SDL_Color{255, 200, 0, 255});
    
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
    
    // Draw Output Text (lock so we don't read mid-update from generation thread)
    std::string output_snapshot;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_snapshot = output_text_;
    }
    DrawText(25, y_start + 35, output_snapshot, {200, 255, 200, 255});
}

void GUIApp::RenderStatusBar() {
    int y_pos = window_height_ - 40;
    
    SDL_SetRenderDrawColor(renderer_, 40, 40, 55, 255);
    SDL_Rect status_bar = {0, y_pos, window_width_, 40};
    SDL_RenderFillRect(renderer_, &status_bar);
    
    // Status Message
    SDL_Color status_color = {150, 200, 150, 255};
    if (state_.load() == GUIState::ERROR) {
        status_color = {255, 100, 100, 255};
    }

    std::string status_snapshot;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_snapshot = status_message_;
    }
    DrawText(15, y_pos + 10, status_snapshot, status_color);
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

        // Model Item Background (use a clear if/else; the previous ternary was malformed)
        if (model.loaded) {
            SDL_SetRenderDrawColor(renderer_, 80, 100, 80, 255);
        } else {
            SDL_SetRenderDrawColor(renderer_, 60, 60, 80, 255);
        }
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
    state_.store(GUIState::LOADING_MODEL);
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_message_ = "Loading model...";
    }

    TurboQuantConfig quant_cfg;
    quant_cfg.use_polar_transform = true;
    quant_cfg.key_bits = kBackgroundQuantKeyBits;
    quant_cfg.value_bits = kBackgroundQuantValueBits;

    // The new OmniEngine API takes a ModelParameters struct rather than a raw path.
    ModelParameters params;
    params.model_path = model_path;
    params.model_name = fs::path(model_path).filename().string();
    params.n_ctx = settings_.n_ctx;
    params.n_batch = settings_.n_batch;
    params.n_gpu_layers = settings_.use_gpu ? settings_.n_gpu_layers : 0;

    if (engine_.LoadModel(params, quant_cfg)) {
        state_.store(GUIState::IDLE);
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_message_ = "Model loaded successfully.";

        // Find and mark as loaded; store an index instead of a pointer that
        // can dangle when the vector is resized.
        for (size_t i = 0; i < available_models_.size(); ++i) {
            if (available_models_[i].path == model_path) {
                available_models_[i].loaded = true;
                current_model_index_ = static_cast<int>(i);
                break;
            }
        }
    } else {
        state_.store(GUIState::ERROR);
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_message_ = "Failed to load model.";
    }
}

void GUIApp::UnloadModel() {
    if (current_model_index_ >= 0 &&
        current_model_index_ < static_cast<int>(available_models_.size())) {
        available_models_[current_model_index_].loaded = false;
    }
    current_model_index_ = -1;
    engine_.UnloadModel();
    std::lock_guard<std::mutex> lock(output_mutex_);
    status_message_ = "Model unloaded.";
}

void GUIApp::GenerateResponse() {
    if (current_model_index_ < 0 ||
        current_model_index_ >= static_cast<int>(available_models_.size()) ||
        !available_models_[current_model_index_].loaded) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_message_ = "No model loaded.";
        return;
    }

    state_.store(GUIState::GENERATING);
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        status_message_ = "Generating response...";
        output_text_.clear();
    }

    // Snapshot inputs needed by the generation thread to avoid races on the
    // members owned by the GUI/main thread.
    const std::string prompt_snapshot = input_prompt_;
    GenerationConfig gen_cfg;
    gen_cfg.temperature = settings_.temperature;
    gen_cfg.top_p = settings_.top_p;
    gen_cfg.max_tokens = settings_.max_tokens;

    // Run generation in background thread
    std::thread gen_thread([this, prompt_snapshot, gen_cfg]() {
        std::string result = engine_.Generate(
            prompt_snapshot,
            gen_cfg,
            [this](const std::string& token) {
                OnTokenGenerated(token);
            }
        );
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_text_ = std::move(result);
        state_.store(GUIState::IDLE);
        status_message_ = "Generation complete.";
    });

    gen_thread.detach();
}

void GUIApp::OnTokenGenerated(const std::string& token) {
    std::lock_guard<std::mutex> lock(output_mutex_);
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