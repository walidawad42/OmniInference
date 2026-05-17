#include "gui_memory_buffer_panel.h"
#include <iostream>
#include <cmath>

GUIMemoryBufferPanel::GUIMemoryBufferPanel() {
    vram_history_.resize(120, 0.0f);
    ram_history_.resize(120, 0.0f);
    utilization_history_.resize(120, 0.0f);

    enable_deterministic_buffer_ = true;
    buffer_size_gb_ = 1;
    overflow_threshold_ = 0.85f;
    enable_ram_overflow_ = true;
    enable_disk_swap_ = true;
}

void GUIMemoryBufferPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(1920, 1000), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("💾 Memory Buffer Management & Overflow Control", nullptr)) {
        ImGui::BeginTabBar("##memory_tabs");

        // TAB 1: Buffer Configuration
        if (ImGui::BeginTabItem("⚙️ Buffer Config")) {
            RenderBufferConfiguration();
            ImGui::EndTabItem();
        }

        // TAB 2: VRAM Status
        if (ImGui::BeginTabItem("🔴 VRAM Status")) {
            RenderVRAMStatus();
            ImGui::EndTabItem();
        }

        // TAB 3: System RAM Status
        if (ImGui::BeginTabItem("🟡 System RAM")) {
            RenderSystemRAMStatus();
            ImGui::EndTabItem();
        }

        // TAB 4: Disk Swap
        if (ImGui::BeginTabItem("🟢 Disk Swap")) {
            RenderDiskSwapStatus();
            ImGui::EndTabItem();
        }

        // TAB 5: Allocations
        if (ImGui::BeginTabItem("📊 Allocations")) {
            RenderAllocationTable();
            ImGui::EndTabItem();
        }

        // TAB 6: Memory Flow
        if (ImGui::BeginTabItem("🌊 Memory Flow")) {
            RenderMemoryFlowVisualization();
            ImGui::EndTabItem();
        }

        // TAB 7: Overflow Simulation
        if (ImGui::BeginTabItem("⚠️ Overflow Sim")) {
            RenderOverflowSimulation();
            ImGui::EndTabItem();
        }

        // TAB 8: Prediction
        if (ImGui::BeginTabItem("🔮 Prediction")) {
            RenderMemoryPrediction();
            ImGui::EndTabItem();
        }

        // TAB 9: Tiering
        if (ImGui::BeginTabItem("🏗️ Memory Tier")) {
            RenderMemoryTiering();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void GUIMemoryBufferPanel::RenderBufferConfiguration() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ DETERMINISTIC BUFFER CONFIGURATION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##buffer_config", ImVec2(0, 300), true);

    // Enable/Disable deterministic buffer
    ImGui::Checkbox("🔒 Enable Deterministic Buffer", &enable_deterministic_buffer_);
    ImGui::SameLine();
    ImGui::HelpMarker("Reserve fixed memory for OS stability");

    if (enable_deterministic_buffer_) {
        ImGui::SliderScalar("Buffer Size (GB)##buffer", ImGuiDataType_U64, &buffer_size_gb_, 0ULL, 16ULL);
        ImGui::SameLine();
        ImGui::HelpMarker("Recommended: 1GB for OS stability");

        if (memory_manager_) {
            uint64_t total_vram = memory_manager_->GetTotalVRAM();
            uint64_t reserved = buffer_size_gb_ * 1024 * 1024 * 1024;
            uint64_t usable = total_vram - reserved;

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Total VRAM: %.2f GB", total_vram / (1024.0*1024.0*1024.0));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Reserved Buffer: %.2f GB", reserved / (1024.0*1024.0*1024.0));
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "Usable VRAM: %.2f GB", usable / (1024.0*1024.0*1024.0));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Overflow configuration
    ImGui::Checkbox("🌊 Enable RAM Overflow", &enable_ram_overflow_);
    ImGui::SameLine();
    ImGui::HelpMarker("Automatically spill to system RAM when VRAM full");

    ImGui::Checkbox("💿 Enable Disk Swap", &enable_disk_swap_);
    ImGui::SameLine();
    ImGui::HelpMarker("Use disk as fallback when RAM full (slow)");

    ImGui::SliderFloat("Overflow Threshold##pct", &overflow_threshold_, 0.5f, 0.95f, "%.0f%%");
    ImGui::SameLine();
    ImGui::HelpMarker("Start overflow at this utilization level");

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("✓ APPLY BUFFER CONFIGURATION", ImVec2(-1, 50))) {
        MemoryPolicy policy;
        policy.vram_buffer_size = buffer_size_gb_ * 1024 * 1024 * 1024;
        policy.utilization_threshold = overflow_threshold_;
        policy.enable_disk_swap = enable_disk_swap_;

        if (memory_manager_) {
            memory_manager_->SetPolicy(policy);
        }

        std::cout << "[GUI] Applied buffer configuration:" << std::endl;
        std::cout << "  - Buffer: " << buffer_size_gb_ << "GB" << std::endl;
        std::cout << "  - Threshold: " << overflow_threshold_ << std::endl;
    }
}

void GUIMemoryBufferPanel::RenderVRAMStatus() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ GPU VRAM (VIDEO MEMORY) STATUS");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!memory_manager_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Memory Manager not initialized");
        return;
    }

    ImGui::BeginChild("##vram_status", ImVec2(0, 200), true);

    uint64_t total_vram = memory_manager_->GetTotalVRAM();
    uint64_t available_vram = memory_manager_->GetAvailableVRAM();
    uint64_t reserved = memory_manager_->GetReservedBuffer();
    uint64_t usable = memory_manager_->GetUsableVRAM();

    float total_gb = total_vram / (1024.0*1024.0*1024.0);
    float available_gb = available_vram / (1024.0*1024.0*1024.0);
    float reserved_gb = reserved / (1024.0*1024.0*1024.0);
    float usable_gb = usable / (1024.0*1024.0*1024.0);

    ImGui::Columns(2, "##vram_cols", true);

    ImGui::Text("Total VRAM:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f GB", total_gb);
    ImGui::NextColumn();

    ImGui::Text("Available:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%.2f GB", available_gb);
    ImGui::NextColumn();

    ImGui::Text("Reserved Buffer:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.2f GB", reserved_gb);
    ImGui::NextColumn();

    ImGui::Text("Usable VRAM:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f GB", usable_gb);
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("📈 VRAM Utilization:");

    ImGui::BeginChild("##vram_graph", ImVec2(0, 200), true);

    float utilization = (total_vram > 0) ? (static_cast<float>(total_vram - available_vram) / total_vram) * 100.0f : 0.0f;

    ImGui::ProgressBar(utilization / 100.0f, ImVec2(-1, 25));
    ImGui::SameLine();
    ImGui::Text("%.1f%%", utilization);

    ImGui::PlotLines("##vram_util", vram_history_.data(), vram_history_.size(), 0, "VRAM %", 0.0f, 100.0f, ImVec2(-1, 150));

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("🔄 REFRESH VRAM STATUS", ImVec2(-1, 40))) {
        UpdateHistories();
    }
}

void GUIMemoryBufferPanel::RenderSystemRAMStatus() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ SYSTEM RAM STATUS (FOR OVERFLOW)");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!memory_manager_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Memory Manager not initialized");
        return;
    }

    ImGui::BeginChild("##ram_status", ImVec2(0, 250), true);

    uint64_t total_ram = memory_manager_->GetTotalSystemRAM();
    uint64_t available_ram = memory_manager_->GetAvailableSystemRAM();

    ImGui::Columns(2, "##ram_cols", true);

    ImGui::Text("Total System RAM:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%.2f GB", total_ram / (1024.0*1024.0*1024.0));
    ImGui::NextColumn();

    ImGui::Text("Available RAM:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f GB", available_ram / (1024.0*1024.0*1024.0));
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Separator();

    ImGui::Text("📊 RAM Allocation Graph:");

    ImGui::PlotLines("##ram_util", ram_history_.data(), ram_history_.size(), 0, "RAM MB", 0.0f, 
                    static_cast<float>(total_ram / (1024*1024)), ImVec2(-1, 150));

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "⚠️ Using System RAM for VRAM overflow SLOWS DOWN inference significantly!");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "💡 Tip: Use smaller context size or 2-bit quantization to fit in VRAM");
}

void GUIMemoryBufferPanel::RenderDiskSwapStatus() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ DISK SWAP STATUS (LAST RESORT)");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!memory_manager_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Memory Manager not initialized");
        return;
    }

    ImGui::BeginChild("##swap_status", ImVec2(0, 200), true);

    uint64_t swap_used = memory_manager_->GetSwapUsage();

    ImGui::Checkbox("Enable Disk Swap", &enable_disk_swap_);
    ImGui::SameLine();
    ImGui::HelpMarker("EXTREMELY SLOW - only use as last resort");

    ImGui::Text("Swap Used: %.2f GB", swap_used / (1024.0*1024.0*1024.0));

    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⛔ DISK SWAP IS 100-1000x SLOWER THAN GPU VRAM!");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "💡 Only use this for very large models with adequate time");

    ImGui::EndChild();

    ImGui::Separator();

    static char swap_path[256] = "./swap";
    ImGui::InputText("Swap Directory", swap_path, sizeof(swap_path));

    if (ImGui::Button("Enable Disk Swap at Path", ImVec2(-1, 40))) {
        if (memory_manager_) {
            memory_manager_->EnableDiskSwap(swap_path);
        }
    }
}

void GUIMemoryBufferPanel::RenderAllocationTable() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY ALLOCATIONS");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!memory_manager_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Memory Manager not initialized");
        return;
    }

    ImGui::BeginChild("##allocations", ImVec2(0, 500), true);

    auto allocations = memory_manager_->GetAllocations();

    ImGui::Columns(5, "##alloc_cols", true);
    ImGui::Text("Allocation ID");
    ImGui::NextColumn();
    ImGui::Text("Size (MB)");
    ImGui::NextColumn();
    ImGui::Text("Offset");
    ImGui::NextColumn();
    ImGui::Text("Purpose");
    ImGui::NextColumn();
    ImGui::Text("Action");
    ImGui::NextColumn();

    ImGui::Separator();

    for (const auto& alloc : allocations) {
        ImGui::Text("%s", alloc.allocation_id.c_str());
        ImGui::NextColumn();

        ImGui::Text("%.2f", alloc.size_bytes / (1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("0x%lX", alloc.offset);
        ImGui::NextColumn();

        ImGui::Text("%s", alloc.purpose.c_str());
        ImGui::NextColumn();

        if (ImGui::Button(("Free##" + alloc.allocation_id).c_str())) {
            if (memory_manager_) {
                memory_manager_->DeallocateVRAM(alloc.allocation_id);
            }
        }

        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    ImGui::EndChild();
}

void GUIMemoryBufferPanel::RenderMemoryFlowVisualization() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY HIERARCHY & OVERFLOW FLOW");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##memory_flow", ImVec2(0, 500), true);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    draw_list->AddRectFilled(canvas_pos, canvas_pos + canvas_size, ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f)));

    // Draw memory tiers
    // Tier 1: GPU VRAM
    ImVec2 tier1_pos(canvas_pos.x + 50, canvas_pos.y + 50);
    ImVec2 tier1_size(150, 100);
    draw_list->AddRectFilled(tier1_pos, tier1_pos + tier1_size, ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.2f, 1.0f)));
    draw_list->AddText(tier1_pos + ImVec2(30, 40), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "GPU VRAM");
    draw_list->AddText(tier1_pos + ImVec2(20, 65), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "(Fastest)");

    // Arrow 1: VRAM -> RAM
    ImVec2 arrow1_start(tier1_pos.x + tier1_size.x, tier1_pos.y + tier1_size.y / 2);
    ImVec2 arrow1_end(tier1_pos.x + tier1_size.x + 100, tier1_pos.y + tier1_size.y / 2);
    draw_list->AddLine(arrow1_start, arrow1_end, ImGui::GetColorU32(ImVec4(1, 1, 0, 1)), 2);
    draw_list->AddTriangleFilled(arrow1_end, arrow1_end + ImVec2(-10, -5), arrow1_end + ImVec2(-10, 5), 
                                ImGui::GetColorU32(ImVec4(1, 1, 0, 1)));

    // Tier 2: System RAM
    ImVec2 tier2_pos(arrow1_end.x + 20, tier1_pos.y);
    ImVec2 tier2_size(150, 100);
    draw_list->AddRectFilled(tier2_pos, tier2_pos + tier2_size, ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.2f, 1.0f)));
    draw_list->AddText(tier2_pos + ImVec2(25, 40), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "System RAM");
    draw_list->AddText(tier2_pos + ImVec2(15, 65), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "(Slower)");

    // Arrow 2: RAM -> DISK
    ImVec2 arrow2_start(tier2_pos.x + tier2_size.x, tier2_pos.y + tier2_size.y / 2);
    ImVec2 arrow2_end(tier2_pos.x + tier2_size.x + 100, tier2_pos.y + tier2_size.y / 2);
    draw_list->AddLine(arrow2_start, arrow2_end, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), 2);
    draw_list->AddTriangleFilled(arrow2_end, arrow2_end + ImVec2(-10, -5), arrow2_end + ImVec2(-10, 5), 
                                ImGui::GetColorU32(ImVec4(1, 0, 0, 1)));

    // Tier 3: Disk Swap
    ImVec2 tier3_pos(arrow2_end.x + 20, tier2_pos.y);
    ImVec2 tier3_size(150, 100);
    draw_list->AddRectFilled(tier3_pos, tier3_pos + tier3_size, ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f)));
    draw_list->AddText(tier3_pos + ImVec2(35, 40), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "Disk");
    draw_list->AddText(tier3_pos + ImVec2(20, 65), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "(Slowest)");

    // Add 1GB reserved buffer indicator
    ImVec2 buffer_pos(canvas_pos.x + 50, canvas_pos.y + 200);
    draw_list->AddRectFilled(buffer_pos, buffer_pos + ImVec2(400, 80), ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 0.2f, 0.3f)));
    draw_list->AddText(buffer_pos + ImVec2(100, 30), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), "🔒 DETERMINISTIC BUFFER (1GB)");

    ImGui::Dummy(ImVec2(0, 350));

    ImGui::EndChild();
}

void GUIMemoryBufferPanel::RenderOverflowSimulation() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY OVERFLOW SIMULATION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    static int sim_model_size_gb = 7;
    static int sim_context_len = 4096;
    static bool run_simulation = false;

    ImGui::SliderInt("Model Size (GB)##sim", &sim_model_size_gb, 1, 70);
    ImGui::SliderInt("Context Length##sim", &sim_context_len, 512, 131072);

    ImGui::Spacing();

    if (ImGui::Button("▶ RUN OVERFLOW SIMULATION", ImVec2(-1, 50))) {
        run_simulation = true;

        if (memory_manager_) {
            uint64_t model_bytes = sim_model_size_gb * 1024UL * 1024UL * 1024UL;
            uint64_t kv_cache_bytes = memory_manager_->EstimateKVCacheSize(32, 4096, sim_context_len);
            uint64_t total_required = model_bytes + kv_cache_bytes;

            uint64_t vram_available = memory_manager_->GetUsableVRAM();
            uint64_t ram_available = memory_manager_->GetTotalSystemRAM();

            std::cout << "[Simulation] Model: " << sim_model_size_gb << "GB" << std::endl;
            std::cout << "[Simulation] KV-Cache: " << (kv_cache_bytes / (1024*1024*1024)) << "GB" << std::endl;
            std::cout << "[Simulation] Total: " << (total_required / (1024*1024*1024)) << "GB" << std::endl;
            std::cout << "[Simulation] VRAM Available: " << (vram_available / (1024*1024*1024)) << "GB" << std::endl;

            if (total_required <= vram_available) {
                std::cout << "[Simulation] ✓ FITS IN VRAM - No overflow needed" << std::endl;
            } else if (total_required <= vram_available + ram_available) {
                uint64_t overflow_size = total_required - vram_available;
                std::cout << "[Simulation] ⚠️ WILL OVERFLOW TO RAM: " << (overflow_size / (1024*1024*1024)) << "GB" << std::endl;
            } else {
                uint64_t deficit = total_required - vram_available - ram_available;
                std::cout << "[Simulation] ✗ EXCEEDS AVAILABLE MEMORY BY: " << (deficit / (1024*1024*1024)) << "GB" << std::endl;
            }
        }
    }

    ImGui::Spacing();

    if (run_simulation && memory_manager_) {
        ImGui::Separator();

        uint64_t model_bytes = sim_model_size_gb * 1024UL * 1024UL * 1024UL;
        uint64_t kv_cache_bytes = memory_manager_->EstimateKVCacheSize(32, 4096, sim_context_len);
        uint64_t total_required = model_bytes + kv_cache_bytes;

        uint64_t vram_available = memory_manager_->GetUsableVRAM();
        uint64_t ram_available = memory_manager_->GetTotalSystemRAM();

        ImGui::BeginChild("##sim_results", ImVec2(0, 300), true);

        ImGui::Text("Simulation Results:");

        ImGui::Columns(2, "##sim_cols", true);

        ImGui::Text("Model Size:");
        ImGui::NextColumn();
        ImGui::Text("%d GB", sim_model_size_gb);
        ImGui::NextColumn();

        ImGui::Text("KV-Cache (FP16):");
        ImGui::NextColumn();
        ImGui::Text("%.2f GB", kv_cache_bytes / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("Total Required:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.2f GB", total_required / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("VRAM Available:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f GB", vram_available / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("RAM Available:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%.2f GB", ram_available / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Separator();

        if (total_required <= vram_available) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "✓ OPTIMAL: Fits in VRAM entirely");
        } else if (total_required <= vram_available + ram_available) {
            uint64_t overflow = total_required - vram_available;
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "⚠️ OVERFLOW: %.2f GB will spill to RAM", overflow / (1024.0*1024.0*1024.0));
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "   Performance will be significantly reduced!");
        } else {
            uint64_t deficit = total_required - vram_available - ram_available;
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ INSUFFICIENT: Exceeds by %.2f GB", deficit / (1024.0*1024.0*1024.0));
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "   Reduce context or use quantization!");
        }

        ImGui::EndChild();
    }
}

void GUIMemoryBufferPanel::RenderMemoryPrediction() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY REQUIREMENT PREDICTOR");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    static int pred_n_layers = 32;
    static int pred_n_embd = 4096;
    static int pred_context = 4096;
    static int pred_quant_bits = 16; // FP16 default

    ImGui::SliderInt("Layers##pred", &pred_n_layers, 8, 128);
    ImGui::SliderInt("Embedding Dim##pred", &pred_n_embd, 256, 16384);
    ImGui::SliderInt("Context Length##pred", &pred_context, 512, 131072);

    ImGui::SliderInt("Data Type##pred", &pred_quant_bits, 2, 32);
    ImGui::SameLine();
    ImGui::HelpMarker("2=2-bit quant, 4=4-bit quant, 8=int8, 16=FP16, 32=FP32");

    ImGui::Separator();

    if (memory_manager_) {
        // Calculate KV cache
        uint64_t kv_cache = static_cast<uint64_t>(2 * pred_n_layers * pred_n_embd * pred_context * (pred_quant_bits / 8.0f));

        // Rough model size estimate
        uint64_t model_estimate = static_cast<uint64_t>(pred_n_layers * pred_n_embd * 4 * 4); // Very rough

        uint64_t total = model_estimate + kv_cache;

        ImGui::Columns(2, "##pred_cols", true);

        ImGui::Text("Model (estimate):");
        ImGui::NextColumn();
        ImGui::Text("%.2f GB", model_estimate / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("KV-Cache:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f GB", kv_cache / (1024.0*1024.0*1024.0));
        ImGui::NextColumn();

        ImGui::Text("Total Required:");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%.2f GB", total / (1024.0*1024.0*1024.0));
        ImGui::NextColumn;

        ImGui::Columns(1);
    }
}

void GUIMemoryBufferPanel::RenderMemoryTiering() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ MEMORY TIERING STRATEGY");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    static int alloc_size_mb = 512;
    ImGui::SliderInt("Allocation Size (MB)##tier", &alloc_size_mb, 1, 8192);

    if (memory_manager_) {
        uint64_t alloc_bytes = static_cast<uint64_t>(alloc_size_mb) * 1024 * 1024;
        MemoryTier best_tier = memory_manager_->GetBestTierForAllocation(alloc_bytes);

        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "Recommended Tier: ");
        ImGui::SameLine();

        switch (best_tier) {
            case MemoryTier::GPU_VRAM:
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "GPU VRAM (Fastest)");
                break;
            case MemoryTier::SYSTEM_RAM:
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "System RAM (Medium)");
                break;
            case MemoryTier::DISK_SWAP:
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disk Swap (Slowest)");
                break;
        }
    }
}

void GUIMemoryBufferPanel::UpdateHistories() {
    if (!memory_manager_) return;

    // Update VRAM history
    uint64_t total_vram = memory_manager_->GetTotalVRAM();
    uint64_t available_vram = memory_manager_->GetAvailableVRAM();
    float vram_util = (total_vram > 0) ? ((total_vram - available_vram) / static_cast<float>(total_vram)) * 100.0f : 0.0f;

    vram_history_.erase(vram_history_.begin());
    vram_history_.push_back(vram_util);

    // Update RAM history
    uint64_t total_ram = memory_manager_->GetTotalSystemRAM();
    uint64_t available_ram = memory_manager_->GetAvailableSystemRAM();
    float ram_mb = (total_ram - available_ram) / (1024.0*1024.0);

    ram_history_.erase(ram_history_.begin());
    ram_history_.push_back(ram_mb);
}

ImVec4 GUIMemoryBufferPanel::GetTierColor(MemoryTier tier) {
    switch (tier) {
        case MemoryTier::GPU_VRAM: return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        case MemoryTier::SYSTEM_RAM: return ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
        case MemoryTier::DISK_SWAP: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

std::string GUIMemoryBufferPanel::FormatBytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + "B";
    if (bytes < 1024*1024) return std::to_string(bytes / 1024) + "KB";
    if (bytes < 1024*1024*1024) return std::to_string(bytes / (1024*1024)) + "MB";
    return std::to_string(bytes / (1024*1024*1024)) + "GB";
}