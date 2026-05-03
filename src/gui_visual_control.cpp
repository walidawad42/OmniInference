#include "gui_visual_control.h"
#include <cmath>
#include <iostream>
#include <algorithm>

GUIVisualControl::GUIVisualControl() {
    // Initialize model parameters with defaults
    current_model_params_.n_embd = 4096;
    current_model_params_.n_layer = 32;
    current_model_params_.n_head = 32;
    current_model_params_.n_head_kv = 8;
    current_model_params_.n_ctx = 4096;
    current_model_params_.n_batch = 512;
    current_model_params_.rope_freq_base = 500000.0f;
    current_model_params_.rope_freq_scale = 1.0f;

    current_gen_config_.temperature = 0.7f;
    current_gen_config_.top_p = 0.95f;
    current_gen_config_.top_k = 40.0f;
    current_gen_config_.max_tokens = 512;

    current_quant_config_.mode = QuantizationMode::TURBO_QUANT_3BIT;
    current_quant_config_.key_bits = 4;
    current_quant_config_.value_bits = 2;
    current_quant_config_.use_polar_transform = true;
    current_quant_config_.use_qjl_correction = true;

    // Initialize history vectors
    memory_history_.resize(120, 0.0f);
    speed_history_.resize(120, 0.0f);
}

void GUIVisualControl::RenderFullControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(0, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize - ImVec2(0, 35), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("OmniInference Master Control Panel", nullptr, ImGuiWindowFlags_NoMove)) {
        ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None);

        // TAB 1: HARDWARE SELECTION
        if (ImGui::BeginTabItem("🖥️ Hardware")) {
            RenderHardwareSelectionPanel();
            ImGui::EndTabItem();
        }

        // TAB 2: MODEL LOADING
        if (ImGui::BeginTabItem("📁 Model Loader")) {
            RenderModelLoadingPanel();
            ImGui::EndTabItem();
        }

        // TAB 3: QUANTIZATION
        if (ImGui::BeginTabItem("⚙️ Quantization")) {
            RenderQuantizationControlPanel();
            ImGui::EndTabItem();
        }

        // TAB 4: INFERENCE PARAMETERS
        if (ImGui::BeginTabItem("🔧 Inference")) {
            RenderInferenceParametersPanel();
            ImGui::EndTabItem();
        }

        // TAB 5: GENERATION
        if (ImGui::BeginTabItem("💬 Generation")) {
            RenderGenerationParametersPanel();
            ImGui::EndTabItem();
        }

        // TAB 6: PIPELINE
        if (ImGui::BeginTabItem("🔗 Pipeline")) {
            RenderPipelineVisualizerPanel();
            ImGui::EndTabItem();
        }

        // TAB 7: MEMORY OPTIMIZER
        if (ImGui::BeginTabItem("💾 Memory")) {
            RenderMemoryOptimizerPanel();
            ImGui::EndTabItem();
        }

        // TAB 8: MONITORING
        if (ImGui::BeginTabItem("📊 Monitor")) {
            RenderLiveMonitoringPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void GUIVisualControl::RenderHardwareSelectionPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15, 15));

    // SECTION: Hardware Backend Selection
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ BACKEND SELECTION");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    static int backend_selection = 0;
    const char* backends[] = { "NVIDIA CUDA", "AMD Vulkan", "Intel Vulkan", "Intel SYCL", "CPU Fallback" };

    ImGui::Columns(5, "##backends", true);
    for (int i = 0; i < 5; i++) {
        if (i > 0) ImGui::NextColumn();
        
        ImVec4 button_color = (backend_selection == i) ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.6f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        
        if (ImGui::Button(backends[i], ImVec2(-1, 50))) {
            backend_selection = i;
            if (engine_) {
                HardwareBackend selected = static_cast<HardwareBackend>(i);
                engine_->InitializeBackend(selected);
            }
        }
        
        ImGui::PopStyleColor();
    }
    ImGui::Columns(1);

    ImGui::Separator();

    // SECTION: Detected Hardware Info
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ DETECTED HARDWARE");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    if (engine_) {
        const auto& hw = engine_->GetHardwareProfile();

        ImGui::BeginChild("##hardware_info", ImVec2(0, 200), true, ImGuiWindowFlags_NoMove);

        RenderInfoBox("Device Name", hw.device_name, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        RenderInfoBox("VRAM Total", std::to_string(static_cast<int>(hw.vram_total_bytes / (1024*1024*1024))) + " GB", 
                     ImVec4(0.3f, 0.5f, 0.9f, 1.0f));

        if (hw.backend == HardwareBackend::NVIDIA_CUDA) {
            RenderInfoBox("Compute Capability", 
                         std::to_string(hw.cuda_capability_major) + "." + std::to_string(hw.cuda_capability_minor),
                         ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            RenderInfoBox("CUDA Version", hw.cuda_version, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        }

        ImGui::Separator();

        ImGui::Text("Capabilities:");
        RenderStatusIndicator("FP16 Support", hw.supports_fp16, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::SameLine();
        RenderStatusIndicator("Tensor Cores", hw.supports_tensor_cores, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::SameLine();
        RenderStatusIndicator("Flash Attention", hw.supports_flash_attention, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

        ImGui::EndChild();
    }

    ImGui::PopStyleVar(2);
}

void GUIVisualControl::RenderModelLoadingPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ MODEL SELECTION & LOADING");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // Model browser
    ImGui::Text("Available Models:");

    ImGui::BeginChild("##model_browser", ImVec2(0, 200), true);

    static std::vector<std::string> models = {
        "llama-2-7b.gguf",
        "llama-2-13b.gguf",
        "mistral-7b.gguf",
        "neural-chat-7b.gguf",
        "qwen-vl-chat.gguf",
        "llava-v1.5.gguf"
    };

    for (size_t i = 0; i < models.size(); i++) {
        bool selected = (selected_model_ == static_cast<int>(i));
        
        ImVec4 item_color = selected ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.5f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, item_color);
        
        if (ImGui::Button(models[i].c_str(), ImVec2(-1, 35))) {
            selected_model_ = i;
        }
        
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Model parameter configuration
    ImGui::Text("📋 Model Parameters:");

    ImGui::BeginChild("##model_params", ImVec2(0, 250), true);

    ImGui::SliderInt("Embedding Dimension", &current_model_params_.n_embd, 256, 8192);
    ImGui::SliderInt("Number of Layers", &current_model_params_.n_layer, 8, 128);
    ImGui::SliderInt("Attention Heads", &current_model_params_.n_head, 8, 256);
    ImGui::SliderInt("KV Heads", &current_model_params_.n_head_kv, 1, 64);
    ImGui::SliderInt("Vocabulary Size", &current_model_params_.n_vocab, 1024, 256000);
    ImGui::SliderInt("Context Length", &current_model_params_.n_ctx, 512, 131072);
    ImGui::SliderInt("Batch Size", &current_model_params_.n_batch, 32, 2048);

    ImGui::EndChild();

    ImGui::Separator();

    // RoPE Configuration
    ImGui::Text("🔄 RoPE (Rotary Position Embedding):");

    ImGui::BeginChild("##rope_config", ImVec2(0, 100), true);

    ImGui::SliderFloat("RoPE Frequency Base", &current_model_params_.rope_freq_base, 1e3, 1e6);
    ImGui::SliderFloat("RoPE Frequency Scale", &current_model_params_.rope_freq_scale, 0.1f, 10.0f);

    ImGui::EndChild();

    ImGui::Separator();

    // Load/Unload buttons
    ImGui::Spacing();

    if (ImGui::Button("🔄 LOAD MODEL", ImVec2(200, 50))) {
        current_model_params_.model_name = selected_model_ >= 0 ? models[selected_model_] : "None";
        if (engine_) {
            engine_->LoadModel(current_model_params_, current_quant_config_);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("⛔ UNLOAD MODEL", ImVec2(200, 50))) {
        if (engine_) {
            engine_->UnloadModel();
        }
    }
}

void GUIVisualControl::RenderQuantizationControlPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ TURBOQUANT PLUS CONFIGURATION");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // Quantization Mode Selection
    ImGui::Text("📊 Quantization Mode:");

    ImGui::BeginChild("##quant_mode", ImVec2(0, 250), true);

    static int quant_mode = 2; // 3-bit default

    struct QuantMode {
        const char* name;
        const char* description;
        float ratio;
        int bits;
    };

    QuantMode modes[] = {
        {"2-bit (Max Compression)", "Smallest VRAM footprint, some quality loss", 0.125f, 2},
        {"3-bit (Balanced)", "Best quality-to-size ratio, recommended", 0.1875f, 3},
        {"4-bit (High Quality)", "Minimal quality loss, larger size", 0.25f, 4},
        {"6-bit (Near Lossless)", "Nearly identical to FP16", 0.375f, 6},
        {"8-bit (Lossless)", "Full precision, largest size", 0.5f, 8}
    };

    for (int i = 0; i < 5; i++) {
        ImVec4 button_color = (quant_mode == i) ? ImVec4(0.1f, 0.8f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.5f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, button_color);

        if (ImGui::Button(modes[i].name, ImVec2(-1, 40))) {
            quant_mode = i;
            current_quant_config_.mode = static_cast<QuantizationMode>(i);
        }

        ImGui::PopStyleColor();

        ImGui::Text("└─ %s (Reduction: %.1f%%)", modes[i].description, modes[i].ratio * 100);
        ImGui::Separator();
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Advanced Quantization Settings
    ImGui::Text("⚙️ Advanced Quantization Settings:");

    ImGui::BeginChild("##quant_advanced", ImVec2(0, 200), true);

    ImGui::SliderInt("Key Precision (bits)", &current_quant_config_.key_bits, 2, 8);
    ImGui::SameLine();
    ImGui::HelpMarker("Higher = Better key accuracy for needle-in-haystack tasks");

    ImGui::SliderInt("Value Precision (bits)", &current_quant_config_.value_bits, 2, 8);
    ImGui::SameLine();
    ImGui::HelpMarker("Higher = Better value reconstruction");

    ImGui::Checkbox("Enable Polar Transform (PolarQuant)", &current_quant_config_.use_polar_transform);
    ImGui::SameLine();
    ImGui::HelpMarker("Geometric transformation for better precision");

    ImGui::Checkbox("Enable QJL Correction", &current_quant_config_.use_qjl_correction);
    ImGui::SameLine();
    ImGui::HelpMarker("Johnson-Lindenstrauss residual correction");

    ImGui::Checkbox("Enable Block Sparse", &current_quant_config_.enable_block_sparse);
    ImGui::SameLine();
    ImGui::HelpMarker("Skip low-importance blocks");

    ImGui::SliderInt("Block Size", &current_quant_config_.block_size, 32, 256);

    ImGui::EndChild();

    ImGui::Separator();

    // Apply Quantization
    if (ImGui::Button("✓ APPLY QUANTIZATION", ImVec2(-1, 50))) {
        if (engine_) {
            engine_->ApplyQuantization(current_quant_config_);
        }
    }
}

void GUIVisualControl::RenderInferenceParametersPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ INFERENCE ENGINE CONFIGURATION");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // GPU Layer Configuration
    ImGui::Text("🎯 GPU Acceleration:");

    ImGui::BeginChild("##gpu_config", ImVec2(0, 150), true);

    ImGui::SliderInt("GPU Layers (Offload)", &current_model_params_.n_gpu_layers, -1, 100);
    ImGui::SameLine();
    ImGui::HelpMarker("-1 = Auto, 0 = CPU only, 100 = Full GPU");

    ImGui::SliderInt("Threads", &current_model_params_.threads, 1, 16);

    ImGui::Checkbox("Use Memory Mapping (mmap)", &current_model_params_.use_mmap);
    ImGui::SameLine();
    ImGui::HelpMarker("Faster loading, lower memory footprint");

    ImGui::Checkbox("Lock Model in RAM (mlock)", &current_model_params_.use_mlock);
    ImGui::SameLine();
    ImGui::HelpMarker("Prevents swapping, ensures consistent performance");

    ImGui::EndChild();

    ImGui::Separator();

    // Attention Mechanisms
    ImGui::Text("🧠 Attention Mechanisms:");

    ImGui::BeginChild("##attention", ImVec2(0, 120), true);

    ImGui::Checkbox("Flash Attention v2", &current_model_params_.flash_attn);
    ImGui::SameLine();
    ImGui::HelpMarker("Faster attention computation, requires compatible GPU");

    bool attention_gated = current_gen_config_.use_attention_gated_delta;
    ImGui::Checkbox("Attention Gated Delta Net", &attention_gated);
    ImGui::SameLine();
    ImGui::HelpMarker("Adaptive attention for long contexts (+20% speed)");
    current_gen_config_.use_attention_gated_delta = attention_gated;

    ImGui::EndChild();

    ImGui::Separator();

    // Context Optimization
    ImGui::Text("📖 Context Optimization:");

    ImGui::BeginChild("##context_opt", ImVec2(0, 100), true);

    static int context_presets = 0;
    const char* presets[] = { "4K (Fast)", "8K (Balanced)", "32K (Quality)", "128K (Slow)" };
    int preset_sizes[] = { 4096, 8192, 32768, 131072 };

    if (ImGui::Combo("##context_preset", &context_presets, presets, 4)) {
        current_model_params_.n_ctx = preset_sizes[context_presets];
    }

    ImGui::SliderInt("Manual Context", &current_model_params_.n_ctx, 512, 131072, "%d tokens");

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("✓ APPLY INFERENCE SETTINGS", ImVec2(-1, 50))) {
        // Apply settings
    }
}

void GUIVisualControl::RenderGenerationParametersPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ TEXT GENERATION PARAMETERS");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // Sampling Method
    ImGui::Text("🎲 Sampling Configuration:");

    ImGui::BeginChild("##sampling", ImVec2(0, 180), true);

    ImGui::SliderFloat("Temperature", &current_gen_config_.temperature, 0.0f, 2.0f, "%.2f");
    ImGui::SameLine();
    ImGui::HelpMarker("0.0 = Deterministic, 1.0 = Normal, 2.0 = Very random");

    ImGui::SliderFloat("Top-P (Nucleus)", &current_gen_config_.top_p, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::HelpMarker("Cumulative probability threshold");

    ImGui::SliderFloat("Top-K", &current_gen_config_.top_k, 0.0f, 100.0f, "%.0f");
    ImGui::SameLine();
    ImGui::HelpMarker("Only sample from top K tokens");

    ImGui::SliderFloat("Min-P", &current_gen_config_.min_p, 0.0f, 1.0f, "%.3f");
    ImGui::SameLine();
    ImGui::HelpMarker("Minimum probability threshold");

    ImGui::EndChild();

    ImGui::Separator();

    // Length Control
    ImGui::Text("📏 Generation Length:");

    ImGui::BeginChild("##length", ImVec2(0, 100), true);

    ImGui::SliderInt("Max Tokens", &current_gen_config_.max_tokens, 1, 8192);

    static int length_preset = 1;
    const char* length_presets[] = { "Short (128)", "Medium (512)", "Long (2048)", "Very Long (8192)" };
    int length_values[] = { 128, 512, 2048, 8192 };

    if (ImGui::Combo("##length_preset", &length_preset, length_presets, 4)) {
        current_gen_config_.max_tokens = length_values[length_preset];
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Advanced Generation
    ImGui::Text("🔬 Advanced Sampling:");

    ImGui::BeginChild("##advanced_gen", ImVec2(0, 100), true);

    ImGui::Checkbox("Use Attention Gated Delta", &current_gen_config_.use_attention_gated_delta);
    ImGui::SameLine();
    ImGui::HelpMarker("Improves long-context coherence");

    ImGui::SliderInt("Sparse Threshold", &current_gen_config_.sparse_threshold, 0, 100);
    ImGui::SameLine();
    ImGui::HelpMarker("0 = Disabled, Higher = More aggressive sparsity");

    ImGui::EndChild();

    ImGui::Separator();

    // Presets
    ImGui::Text("🎯 Quick Presets:");

    static int gen_preset = 0;
    const char* gen_presets[] = { "Creative", "Balanced", "Deterministic", "Expert" };

    ImGui::RadioButton("Creative (T=1.2, P=0.95)", &gen_preset, 0);
    if (gen_preset == 0) {
        current_gen_config_.temperature = 1.2f;
        current_gen_config_.top_p = 0.95f;
    }

    ImGui::RadioButton("Balanced (T=0.7, P=0.95)", &gen_preset, 1);
    if (gen_preset == 1) {
        current_gen_config_.temperature = 0.7f;
        current_gen_config_.top_p = 0.95f;
    }

    ImGui::RadioButton("Deterministic (T=0.0, P=1.0)", &gen_preset, 2);
    if (gen_preset == 2) {
        current_gen_config_.temperature = 0.0f;
        current_gen_config_.top_p = 1.0f;
    }

    ImGui::RadioButton("Expert (T=0.5, P=0.9)", &gen_preset, 3);
    if (gen_preset == 3) {
        current_gen_config_.temperature = 0.5f;
        current_gen_config_.top_p = 0.9f;
    }

    ImGui::Separator();

    if (ImGui::Button("✓ SAVE GENERATION CONFIG", ImVec2(-1, 50))) {
        // Save config
    }
}

void GUIVisualControl::RenderPipelineVisualizerPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ INFERENCE PIPELINE");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // Pipeline flow visualization
    ImGui::BeginChild("##pipeline_viz", ImVec2(0, 300), true);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Draw pipeline stages
    float stage_width = canvas_size.x / 5;
    std::vector<std::string> stages = { "Input", "Tokenize", "Embed", "Inference", "Decode" };
    std::vector<ImVec4> stage_colors = {
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.3f, 0.7f, 0.3f, 1.0f),
        ImVec4(0.4f, 0.6f, 0.4f, 1.0f),
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        ImVec4(0.6f, 0.4f, 0.6f, 1.0f)
    };

    for (size_t i = 0; i < stages.size(); i++) {
        ImVec2 stage_pos(canvas_pos.x + i * stage_width + stage_width / 2 - 40, canvas_pos.y + 50);
        ImVec2 stage_size(80, 60);

        ImU32 stage_color = ImGui::GetColorU32(stage_colors[i]);
        draw_list->AddRectFilled(stage_pos, stage_pos + stage_size, stage_color);
        draw_list->AddRect(stage_pos, stage_pos + stage_size, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2);

        draw_list->AddText(stage_pos + ImVec2(10, 20), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), stages[i].c_str());

        // Draw arrow to next stage
        if (i < stages.size() - 1) {
            ImVec2 arrow_start(stage_pos.x + stage_size.x, stage_pos.y + stage_size.y / 2);
            ImVec2 arrow_end(stage_pos.x + stage_width, stage_pos.y + stage_size.y / 2);
            draw_list->AddLine(arrow_start, arrow_end, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2);
            draw_list->AddTriangleFilled(arrow_end, arrow_end + ImVec2(-10, -5), arrow_end + ImVec2(-10, 5), 
                                        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
        }
    }

    ImGui::Dummy(ImVec2(0, 150));
    ImGui::EndChild();

    ImGui::Separator();

    // Pipeline statistics
    ImGui::Text("📈 Pipeline Statistics:");

    ImGui::BeginChild("##pipeline_stats", ImVec2(0, 150), true);

    ImGui::Columns(2, "##stats", true);

    ImGui::Text("Tokens/Second:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f TPS", current_tps_);
    ImGui::NextColumn();

    ImGui::Text("Memory Usage:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f MB", current_memory_usage_mb_);
    ImGui::NextColumn();

    ImGui::Text("Context Utilization:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "45.2 %%");
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("▶ START PIPELINE", ImVec2(-1, 50))) {
        // Start pipeline
    }
}

void GUIVisualControl::RenderMemoryOptimizerPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY MANAGEMENT & OPTIMIZATION");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    if (engine_) {
        const auto& hw = engine_->GetHardwareProfile();
        uint64_t total_vram_bytes = hw.vram_total_bytes;
        uint64_t free_vram_bytes = hw.vram_free_bytes;
        float vram_percent = (static_cast<float>(total_vram_bytes - free_vram_bytes) / total_vram_bytes) * 100.0f;

        ImGui::Text("💾 VRAM Status:");
        ImGui::BeginChild("##vram_status", ImVec2(0, 100), true);

        ImGui::Text("Total VRAM: %.2f GB", total_vram_bytes / (1024.0f * 1024.0f * 1024.0f));
        ImGui::Text("Free VRAM: %.2f GB", free_vram_bytes / (1024.0f * 1024.0f * 1024.0f));

        RenderProgressBar("VRAM Usage", vram_percent / 100.0f, ImVec2(-1, 25));

        ImGui::EndChild();
    }

    ImGui::Separator();

    ImGui::Text("🎯 Memory Optimization Strategies:");

    ImGui::BeginChild("##memory_strategies", ImVec2(0, 200), true);

    static int memory_strategy = 0;

    ImVec4 strategy_colors[] = {
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f)
    };

    const char* strategies[] = {
        "🟢 AGGRESSIVE (Max Compression)",
        "🟡 BALANCED (Recommended)",
        "🔴 CONSERVATIVE (Max Quality)",
        "🔵 CUSTOM"
    };

    for (int i = 0; i < 4; i++) {
        ImGui::PushStyleColor(ImGuiCol_Button, strategy_colors[i]);

        if (ImGui::Button(strategies[i], ImVec2(-1, 45))) {
            memory_strategy = i;
        }

        ImGui::PopStyleColor();

        ImGui::Text("└─ ");
        ImGui::SameLine();

        switch (i) {
            case 0:
                ImGui::Text("2-bit quantization, maximum layer offload, sparse attention");
                break;
            case 1:
                ImGui::Text("3-bit quantization, adaptive layer offload, selective sparsity");
                break;
            case 2:
                ImGui::Text("4-bit quantization, minimal offload, no sparsity");
                break;
            case 3:
                ImGui::Text("Custom configuration based on your settings");
                break;
        }

        ImGui::Spacing();
    }

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("⚙️ Fine-tuning Options:");

    ImGui::BeginChild("##memory_tuning", ImVec2(0, 120), true);

    ImGui::SliderInt("KV-Cache Precision", &current_quant_config_.key_bits, 2, 8);
    ImGui::SliderInt("Model Precision", &current_quant_config_.value_bits, 2, 8);

    ImGui::Checkbox("Enable Memory Swapping to Disk", &current_model_params_.use_mmap);
    ImGui::Checkbox("Aggressive Layer Offload", &current_model_params_.use_mlock);

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("🔄 OPTIMIZE & RELOAD", ImVec2(-1, 50))) {
        if (engine_) {
            engine_->OptimizeForHardware(current_model_params_, current_quant_config_);
        }
    }
}

void GUIVisualControl::RenderLiveMonitoringPanel() {
    ImGui::Text("╔══════════════════════════════════════════════════════");
    ImGui::Text("║ REAL-TIME PERFORMANCE MONITORING");
    ImGui::Text("╚══════════════════════════════════════════════════════");

    ImGui::Separator();

    // Live metrics
    ImGui::BeginChild("##metrics", ImVec2(0, 150), true);

    ImGui::Columns(3, "##metrics_cols", true);

    ImGui::Text("Tokens/Second");
    ImGui::NextColumn();
    ImGui::Text("Memory Usage");
    ImGui::NextColumn();
    ImGui::Text("GPU Utilization");
    ImGui::NextColumn();

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f TPS", current_tps_);
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f MB", current_memory_usage_mb_);
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "78%%");
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    // Performance graphs
    ImGui::Text("📊 Performance History:");

    ImGui::BeginChild("##graphs", ImVec2(0, 300), true);

    if (ImGui::BeginTabBar("##monitor_tabs")) {
        if (ImGui::BeginTabItem("Speed (TPS)")) {
            ImGui::PlotLines("##speed_graph", speed_history_.data(), speed_history_.size(), 0, 
                           "Speed", 0.0f, 100.0f, ImVec2(-1, 200));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Memory (MB)")) {
            ImGui::PlotLines("##memory_graph", memory_history_.data(), memory_history_.size(), 0,
                           "Memory", 0.0f, 16000.0f, ImVec2(-1, 200));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("⚙️ Monitoring Options:");

    ImGui::BeginChild("##monitor_options", ImVec2(0, 80), true);

    static bool auto_refresh = true;
    ImGui::Checkbox("Auto Refresh", &auto_refresh);

    static float refresh_rate = 1.0f;
    ImGui::SliderFloat("Refresh Rate (Hz)", &refresh_rate, 0.1f, 10.0f);

    ImGui::EndChild();
}

// Helper rendering functions
void GUIVisualControl::RenderPanelHeader(const std::string& title, bool& expanded) {
    ImGui::Text("%s", title.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    if (ImGui::Button(expanded ? "▼" : "▶", ImVec2(25, 25))) {
        expanded = !expanded;
    }
}

void GUIVisualControl::RenderInfoBox(const std::string& label, const std::string& value, ImVec4 color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s: %s", label.c_str(), value.c_str());
    ImGui::PopStyleColor();
}

void GUIVisualControl::RenderStatusIndicator(const std::string& label, bool active, ImVec4 color) {
    ImVec4 indicator_color = active ? color : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, indicator_color);
    ImGui::Button(active ? "✓" : "✗", ImVec2(25, 25));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text("%s", label.c_str());
}

void GUIVisualControl::RenderProgressBar(const std::string& label, float progress, const ImVec2& size) {
    ImGui::ProgressBar(progress, size);
    ImGui::SameLine();
    ImGui::Text("%s (%.1f%%)", label.c_str(), progress * 100.0f);
}

void GUIVisualControl::RenderSlider(const SliderControl& ctrl) {
    ImGui::SliderFloat(ctrl.label.c_str(), ctrl.value, ctrl.min_val, ctrl.max_val, ctrl.format);
}

void GUIVisualControl::RenderCombo(const ComboControl& ctrl) {
    std::vector<const char*> options_cstr;
    for (const auto& opt : ctrl.options) {
        options_cstr.push_back(opt.c_str());
    }
    ImGui::Combo(ctrl.label.c_str(), ctrl.current, options_cstr.data(), options_cstr.size());
}

void GUIVisualControl::RenderCheckbox(const CheckboxControl& ctrl) {
    ImGui::Checkbox(ctrl.label.c_str(), ctrl.value);
}

void GUIVisualControl::RenderButton(const ButtonControl& ctrl) {
    ImGui::PushStyleColor(ImGuiCol_Button, ctrl.color);
    if (ImGui::Button(ctrl.label.c_str(), ImVec2(-1, 50))) {
        ctrl.callback();
    }
    ImGui::PopStyleColor();
}

void GUIVisualControl::RenderColoredButton(const std::string& label, ImVec4 color, std::function<void()> callback) {
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    if (ImGui::Button(label.c_str(), ImVec2(-1, 50))) {
        callback();
    }
    ImGui::PopStyleColor();
}

void GUIVisualControl::RenderToggleButton(const std::string& label, bool& state) {
    ImVec4 color = state ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    if (ImGui::Button(label.c_str(), ImVec2(-1, 50))) {
        state = !state;
    }
    ImGui::PopStyleColor();
}

void GUIVisualControl::UpdateMemoryHistory() {
    memory_history_.erase(memory_history_.begin());
    memory_history_.push_back(current_memory_usage_mb_);
}

void GUIVisualControl::UpdateSpeedHistory() {
    speed_history_.erase(speed_history_.begin());
    speed_history_.push_back(current_tps_);
}

void GUIVisualControl::RenderHardwareIndicator() {
    if (!engine_) return;

    const auto& hw = engine_->GetHardwareProfile();

    ImGui::Begin("Hardware Status", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("Device: %s", hw.device_name.c_str());
    ImGui::End();
}

void GUIVisualControl::RenderMemoryUsageGraph() {
    ImGui::PlotLines("Memory Usage", memory_history_.data(), memory_history_.size(), 0, "", 0.0f, 16000.0f, ImVec2(400, 100));
}

void GUIVisualControl::RenderTokenSpeedometer() {
    ImGui::Text("Speed: %.2f TPS", current_tps_);
}

void GUIVisualControl::RenderQuantizationQualityMeter() {
    ImGui::ProgressBar(static_cast<float>(current_quant_config_.key_bits) / 8.0f, ImVec2(-1, 25));
}