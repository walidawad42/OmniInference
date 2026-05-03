#include "gui_main.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace {
// Append `addition` to `dst` (a fixed-size char buffer) without overflowing,
// always leaving a NUL terminator. Returns true if the full text fit.
bool SafeAppend(char* dst, size_t dst_capacity, const char* addition) {
    if (!dst || dst_capacity == 0 || !addition) return false;
    size_t cur_len = std::strlen(dst);
    if (cur_len >= dst_capacity - 1) return false;
    size_t remaining = dst_capacity - cur_len - 1;
    size_t add_len = std::strlen(addition);
    size_t to_copy = std::min(add_len, remaining);
    std::memcpy(dst + cur_len, addition, to_copy);
    dst[cur_len + to_copy] = '\0';
    return to_copy == add_len;
}

void SafeAssign(char* dst, size_t dst_capacity, const char* src) {
    if (!dst || dst_capacity == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t copy = std::min(std::strlen(src), dst_capacity - 1);
    std::memcpy(dst, src, copy);
    dst[copy] = '\0';
}
} // namespace

GUIMainWindow::GUIMainWindow() {
    ui_model_params_.n_embd = 4096;
    ui_model_params_.n_layer = 32;
    ui_model_params_.n_head = 32;
    ui_model_params_.n_ctx = 4096;

    ui_quant_config_.mode = QuantizationMode::TURBO_QUANT_3BIT;
    ui_quant_config_.key_bits = 4;
    ui_quant_config_.value_bits = 2;
    ui_quant_config_.use_polar_transform = true;
    ui_quant_config_.use_qjl_correction = true;

    ui_gen_config_.temperature = 0.7f;
    ui_gen_config_.top_p = 0.95f;
    ui_gen_config_.max_tokens = 512;
}

GUIMainWindow::~GUIMainWindow() {
    Shutdown();
}

bool GUIMainWindow::Initialize(int width, int height) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create SDL window
    window_ = SDL_CreateWindow(
        "OmniInference - LLM/MLLM Master Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    // Create OpenGL context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    gl_context_ = SDL_GL_CreateContext(window_);

    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io_ = &ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 430");

    // Initialize OmniEngine
    if (!engine_.InitializeBackend()) {
        std::cerr << "Failed to initialize backend" << std::endl;
        return false;
    }

    engine_.PrintHardwareReport();

    // Initialize Node Editor
    node_editor_.Initialize();

    // Scan for models
    ScanModels();

    return true;
}

void GUIMainWindow::Run() {
    running_.store(true);

    while (running_.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (event.type) {
                case SDL_QUIT:
                    running_.store(false);
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_.store(false);
                    }
                    break;
                default:
                    break;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window_);
        ImGui::NewFrame();

        // Render Main UI
        RenderMenuBar();
        RenderGenerationPanel();
        RenderNodeEditor();

        if (show_hardware_info_) RenderHardwareInfoWindow();
        if (show_model_settings_) RenderModelSettingsWindow();

        RenderStatusBar();

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}

void GUIMainWindow::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (gl_context_) SDL_GL_DeleteContext(gl_context_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void GUIMainWindow::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "ESC")) {
                running_.store(false);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hardware Info", nullptr, &show_hardware_info_);
            ImGui::MenuItem("Model Settings", nullptr, &show_model_settings_);
            ImGui::MenuItem("Generation Panel", nullptr, &show_generation_panel_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Pipeline")) {
            if (ImGui::MenuItem("Export Pipeline")) {
                PipelineConfig cfg = node_editor_.ExportPipeline();
                std::cout << "[GUI] Pipeline exported with " << cfg.nodes.size() << " nodes" << std::endl;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void GUIMainWindow::RenderHardwareInfoWindow() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Hardware Information", &show_hardware_info_)) {
        const auto& hw = engine_.GetHardwareProfile();

        ImGui::Text("Device: %s", hw.device_name.c_str());
        ImGui::Text("Backend: ");
        ImGui::SameLine();

        switch (hw.backend) {
            case HardwareBackend::NVIDIA_CUDA:
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "NVIDIA CUDA %s", hw.cuda_version.c_str());
                ImGui::Text("Compute Capability: %d.%d", hw.cuda_capability_major, hw.cuda_capability_minor);
                break;
            case HardwareBackend::AMD_VULKAN:
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "AMD Vulkan");
                break;
            case HardwareBackend::INTEL_VULKAN:
                ImGui::TextColored(ImVec4(0, 0, 1, 1), "Intel Vulkan");
                break;
            default:
                ImGui::Text("Unknown");
        }

        ImGui::Separator();
        ImGui::Text("VRAM: %.2f GB", hw.vram_total_bytes / (1024.0f * 1024.0f * 1024.0f));
        ImGui::Text("FP16 Support: %s", hw.supports_fp16 ? "✓ YES" : "✗ NO");
        ImGui::Text("Tensor Cores: %s", hw.supports_tensor_cores ? "✓ YES" : "✗ NO");
        ImGui::Text("Flash Attention: %s", hw.supports_flash_attention ? "✓ YES" : "✗ NO");

        ImGui::End();
    }
}

void GUIMainWindow::RenderModelSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Model Settings & Quantization", &show_model_settings_)) {
        // Model Selection
        ImGui::Text("Available Models: %zu", available_models_.size());

        if (ImGui::ListBox("##model_list", &selected_model_,
                [](void* data, int idx, const char** out_text) {
                    auto& models = *static_cast<std::vector<std::string>*>(data);
                    if (idx < static_cast<int>(models.size())) {
                        *out_text = models[idx].c_str();
                        return true;
                    }
                    return false;
                }, &available_models_, available_models_.size(), 5)) {
        }

        if (ImGui::Button("Load Selected Model")) {
            LoadSelectedModel();
        }

        ImGui::Separator();

        // Model Parameters
        ImGui::Text("Model Configuration");
        ImGui::SliderInt("Embedding Dimension##n_embd", &ui_model_params_.n_embd, 256, 16384);
        ImGui::SliderInt("Number of Layers##n_layer", &ui_model_params_.n_layer, 1, 128);
        ImGui::SliderInt("Number of Heads##n_head", &ui_model_params_.n_head, 1, 256);
        ImGui::SliderInt("Context Length##n_ctx", &ui_model_params_.n_ctx, 512, 131072);
        ImGui::SliderFloat("RoPE Freq Base##rope_freq", &ui_model_params_.rope_freq_base, 1e3, 2e6);
        ImGui::SliderFloat("RoPE Freq Scale##rope_scale", &ui_model_params_.rope_freq_scale, 0.1f, 10.0f);

        ImGui::Separator();

        // Quantization Settings
        ImGui::Text("TurboQuant Plus Configuration");

        static int quant_mode = 2; // 3-bit default
        ImGui::RadioButton("2-bit (Maximum Compression)", &quant_mode, 0);
        ImGui::RadioButton("3-bit (Balanced)", &quant_mode, 1);
        ImGui::RadioButton("4-bit (High Quality)", &quant_mode, 2);
        ImGui::RadioButton("6-bit (Near Lossless)", &quant_mode, 3);

        ImGui::SliderInt("Key Bits##key_bits", &ui_quant_config_.key_bits, 2, 8);
        ImGui::SliderInt("Value Bits##val_bits", &ui_quant_config_.value_bits, 2, 8);

        ImGui::Checkbox("Enable Polar Transform", &ui_quant_config_.use_polar_transform);
        ImGui::SameLine();
        ImGui::Checkbox("Enable QJL Correction", &ui_quant_config_.use_qjl_correction);

        ImGui::Separator();

        if (ImGui::Button("Apply Settings", ImVec2(200, 0))) {
            UpdateEngineSettings();
        }

        ImGui::End();
    }
}

void GUIMainWindow::RenderGenerationPanel() {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Generation Panel", &show_generation_panel_)) {
        ImGui::Text("Temperature");
        ImGui::SliderFloat("##temperature", &ui_gen_config_.temperature, 0.0f, 2.0f);

        ImGui::Text("Top-P (Nucleus Sampling)");
        ImGui::SliderFloat("##top_p", &ui_gen_config_.top_p, 0.0f, 1.0f);

        ImGui::Text("Top-K");
        ImGui::SliderFloat("##top_k", &ui_gen_config_.top_k, 0.0f, 100.0f);

        ImGui::Text("Max Tokens");
        ImGui::SliderInt("##max_tokens", &ui_gen_config_.max_tokens, 1, 4096);

        ImGui::Checkbox("Use Attention Gated Delta", &ui_gen_config_.use_attention_gated_delta);

        ImGui::Separator();

        ImGui::TextUnformatted("Prompt:");
        ImGui::InputTextMultiline("##prompt", input_prompt_, sizeof(input_prompt_),
                                 ImVec2(-1, 100), ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Generate", ImVec2(150, 0))) {
            ExecuteGeneration();
        }

        ImGui::Separator();

        ImGui::TextUnformatted("Output:");
        ImGui::InputTextMultiline("##output", output_buffer_, sizeof(output_buffer_),
                                 ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);

        ImGui::End();
    }
}

void GUIMainWindow::RenderNodeEditor() {
    node_editor_.Render();
}

void GUIMainWindow::RenderStatusBar() {
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 25), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 25), ImGuiCond_Always);

    ImGui::Begin("Status Bar", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImGui::Text("Model: %s | Backend: %s | VRAM: %.2f GB",
               engine_.IsModelLoaded() ? "LOADED" : "NONE",
               engine_.GetActiveBackend() == HardwareBackend::NVIDIA_CUDA ? "CUDA" : "Vulkan",
               engine_.GetHardwareProfile().vram_total_bytes / (1024.0f * 1024.0f * 1024.0f));

    ImGui::End();
}

void GUIMainWindow::ScanModels() {
    available_models_.clear();

    try {
        for (const auto& entry : fs::directory_iterator("./models")) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find(".gguf") != std::string::npos) {
                    available_models_.push_back(filename);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning models: " << e.what() << std::endl;
    }
}

void GUIMainWindow::LoadSelectedModel() {
    if (selected_model_ < 0 || selected_model_ >= static_cast<int>(available_models_.size())) {
        return;
    }

    std::string model_path = "./models/" + available_models_[selected_model_];
    ui_model_params_.model_path = model_path;
    ui_model_params_.model_name = available_models_[selected_model_];

    std::thread load_thread([this]() {
        if (engine_.LoadModel(ui_model_params_, ui_quant_config_)) {
            std::cout << "[GUI] Model loaded successfully" << std::endl;
        } else {
            std::cerr << "[GUI] Failed to load model" << std::endl;
        }
    });
    load_thread.detach();
}

void GUIMainWindow::ExecuteGeneration() {
    if (!engine_.IsModelLoaded()) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        SafeAssign(output_buffer_, sizeof(output_buffer_), "ERROR: No model loaded");
        return;
    }

    // Snapshot the inputs the worker thread needs to avoid races.
    const std::string prompt_snapshot(input_prompt_);
    const GenerationConfig gen_cfg = ui_gen_config_;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_buffer_[0] = '\0';
    }

    std::thread gen_thread([this, prompt_snapshot, gen_cfg]() {
        std::string result = engine_.Generate(
            prompt_snapshot,
            gen_cfg,
            [this](const std::string& token) {
                std::lock_guard<std::mutex> lock(output_mutex_);
                SafeAppend(output_buffer_, sizeof(output_buffer_), token.c_str());
            }
        );
        std::lock_guard<std::mutex> lock(output_mutex_);
        SafeAssign(output_buffer_, sizeof(output_buffer_), result.c_str());
    });
    gen_thread.detach();
}

void GUIMainWindow::UpdateEngineSettings() {
    if (!engine_.IsModelLoaded()) {
        engine_.LoadModel(ui_model_params_, ui_quant_config_);
    }
    engine_.ApplyQuantization(ui_quant_config_);
}