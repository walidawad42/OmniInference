#include "gui_memory_buffer_panel.h"
#include <iostream>
#include <cmath>

GUIMemoryBufferPanel::GUIMemoryBufferPanel() {
    vram_history_.resize(120, 0.0f);
    ram_history_.resize(120, 0.0f);
    utilization_history_.resize(120, 0.0f);

    enable_deterministic_buffer_ = true;
    buffer_size_gb_ = 1;  // Default 1GB
    overflow_threshold_ = 0.85f;
    enable_ram_overflow_ = true;
    enable_disk_swap_ = false;
}

void GUIMemoryBufferPanel::RenderBufferConfiguration() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ 🔒 DETERMINISTIC BUFFER - USER CONTROLLED");
    ImGui::Text("║ Objective: Maximize TPS + Error Elimination");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##buffer_config", ImVec2(0, 500), true);

    // ============================================================
    // SECTION 1: Enable/Disable Deterministic Buffer
    // ============================================================

    ImGui::Checkbox("🔒 Enable Deterministic Buffer", &enable_deterministic_buffer_);
    ImGui::SameLine();
    ImGui::HelpMarker("Reserve fixed memory for OS stability and error elimination");

    if (enable_deterministic_buffer_) {
        ImGui::Separator();

        // ============================================================
        // SECTION 2: Buffer Size Selection (200MB to 1GB)
        // ============================================================

        ImGui::Text("📊 Buffer Size (200MB - 1GB):");

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));

        // Convert GB to MB for slider
        int buffer_mb = static_cast<int>(buffer_size_gb_ * 1024);
        int buffer_mb_old = buffer_mb;

        ImGui::SliderInt("##buffer_size_slider", &buffer_mb, 200, 1024, "%d MB");

        ImGui::PopStyleColor();

        // Update if changed
        if (buffer_mb != buffer_mb_old) {
            buffer_size_gb_ = static_cast<double>(buffer_mb) / 1024.0;
        }

        // ============================================================
        // SECTION 3: Buffer Warning System
        // ============================================================

        ImGui::Spacing();
        ImGui::Text("⚠️ BUFFER WARNING SYSTEM:");

        if (memory_manager_) {
            uint64_t total_vram = memory_manager_->GetTotalVRAM();
            uint64_t reserved = static_cast<uint64_t>(buffer_size_gb_ * 1024 * 1024 * 1024);
            float buffer_ratio = static_cast<float>(reserved) / total_vram;

            // Display warning based on buffer ratio
            ImVec4 warning_color(0.2f, 0.8f, 0.2f, 1.0f);
            std::string warning_text = "✓ SAFE";

            if (buffer_ratio > 0.5f) {
                warning_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                warning_text = "✓ SAFE - Buffer is optimal for OS stability";
            } else if (buffer_ratio > 0.3f) {
                warning_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
                warning_text = "⚠️ CAUTION - Buffer may be too small for stability";
            } else if (buffer_ratio > 0.1f) {
                warning_color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                warning_text = "⚠️ WARNING - Buffer size is risky!";
            } else {
                warning_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                warning_text = "🔴 CRITICAL - Buffer TOO SMALL! System instability risk!";
            }

            ImGui::PushStyleColor(ImGuiCol_Text, warning_color);
            ImGui::TextWrapped("%s", warning_text.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // ============================================================
            // SECTION 4: Memory Breakdown
            // ============================================================

            ImGui::Text("📋 Memory Breakdown:");

            ImGui::Columns(2, "##buffer_breakdown", true);

            ImGui::Text("Total VRAM:");
            ImGui::NextColumn();
            ImGui::Text("%.2f GB", total_vram / (1024.0*1024.0*1024.0));
            ImGui::NextColumn();

            ImGui::Text("Reserved Buffer:");
            ImGui::NextColumn();
            ImGui::TextColored(warning_color, "%.2f GB (%.1f%%)", buffer_size_gb_, buffer_ratio * 100.0f);
            ImGui::NextColumn();

            uint64_t usable = total_vram - reserved;
            ImGui::Text("Usable for Models:");
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f GB", usable / (1024.0*1024.0*1024.0));
            ImGui::NextColumn();

            ImGui::Columns(1);

            // ============================================================
            // SECTION 5: Recommended Values
            // ============================================================

            ImGui::Separator();
            ImGui::Text("💡 RECOMMENDED VALUES:");

            ImGui::BeginChild("##recommendations", ImVec2(0, 100), true);

            ImGui::Text("🟢 HIGH THROUGHPUT (TPS Focus):");
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "  • 200-300 MB (Risky but fastest)");

            ImGui::Text("🟡 BALANCED:");
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "  • 500-700 MB (Recommended)");

            ImGui::Text("🟢 SAFE:");
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "  • 1000+ MB (Most stable)");

            ImGui::EndChild();

            ImGui::Separator();

            // ============================================================
            // SECTION 6: Preset Buttons
            // ============================================================

            ImGui::Text("⚡ Quick Presets:");

            if (ImGui::Button("⚡ PERFORMANCE (200MB)##preset_fast", ImVec2(-1, 40))) {
                buffer_size_gb_ = 0.2;
                std::cout << "[Memory] Set buffer to 200MB - HIGH TPS RISK!" << std::endl;
            }

            if (ImGui::Button("⚖️ BALANCED (512MB)##preset_balanced", ImVec2(-1, 40))) {
                buffer_size_gb_ = 0.512;
                std::cout << "[Memory] Set buffer to 512MB - Balanced" << std::endl;
            }

            if (ImGui::Button("🔒 STABLE (1GB)##preset_stable", ImVec2(-1, 40))) {
                buffer_size_gb_ = 1.0;
                std::cout << "[Memory] Set buffer to 1GB - Maximum Stability" << std::endl;
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Deterministic buffer DISABLED");
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "⚠️ WARNING: System may become unstable!");
    }

    ImGui::EndChild();

    ImGui::Separator();

    // ============================================================
    // SECTION 7: Overflow Configuration
    // ============================================================

    ImGui::Text("🌊 OVERFLOW CONFIGURATION:");

    ImGui::BeginChild("##overflow_config", ImVec2(0, 200), true);

    ImGui::Checkbox("🌊 Enable RAM Overflow", &enable_ram_overflow_);
    ImGui::SameLine();
    ImGui::HelpMarker("Automatically move data to system RAM when VRAM full");

    if (enable_ram_overflow_) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", "⚠️ RAM overflow reduces TPS by 50-70%");
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "💡 Tip: Use smaller context or quantization to avoid");
    }

    ImGui::Checkbox("💿 Enable Disk Swap", &enable_disk_swap_);
    ImGui::SameLine();
    ImGui::HelpMarker("Use disk as fallback (EXTREMELY SLOW)");

    if (enable_disk_swap_) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "🔴 DISK SWAP: 100-1000x SLOWER - Avoid if possible!");
    }

    ImGui::SliderFloat("Overflow Threshold##pct", &overflow_threshold_, 0.5f, 0.95f, "%.0f%%");
    ImGui::SameLine();
    ImGui::HelpMarker("Start overflow at this VRAM utilization level");

    ImGui::EndChild();

    ImGui::Separator();

    // ============================================================
    // SECTION 8: Apply & Verify
    // ============================================================

    if (ImGui::Button("✓ APPLY BUFFER CONFIGURATION", ImVec2(-1, 50))) {
        MemoryPolicy policy;
        policy.vram_buffer_size = static_cast<uint64_t>(buffer_size_gb_ * 1024 * 1024 * 1024);
        policy.utilization_threshold = overflow_threshold_;
        policy.enable_disk_swap = enable_disk_swap_;
        policy.optimize_for_throughput = true;

        if (memory_manager_) {
            if (memory_manager_->SetDeterministicBuffer(policy.vram_buffer_size)) {
                memory_manager_->SetPolicy(policy);

                std::cout << "[✓ CONFIG APPLIED]" << std::endl;
                std::cout << "  Buffer: " << buffer_size_gb_ << " GB" << std::endl;
                std::cout << "  Overflow Threshold: " << overflow_threshold_ << std::endl;
                std::cout << "  RAM Overflow: " << (enable_ram_overflow_ ? "ENABLED" : "DISABLED") << std::endl;
                std::cout << "  Disk Swap: " << (enable_disk_swap_ ? "ENABLED" : "DISABLED") << std::endl;
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("🔍 VERIFY CONFIGURATION", ImVec2(-1, 50))) {
        if (memory_manager_) {
            auto health_report = memory_manager_->GetMemoryHealthReport();
            std::cout << "[MEMORY HEALTH REPORT]" << std::endl;
            std::cout << health_report.dump(2) << std::endl;
        }
    }
}

void GUIMemoryBufferPanel::RenderVRAMStatus() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ 🔴 GPU VRAM STATUS");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!memory_manager_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Memory Manager not initialized");
        return;
    }

    ImGui::BeginChild("##vram_status", ImVec2(0, 300), true);

    uint64_t total_vram = memory_manager_->GetTotalVRAM();
    uint64_t available_vram = memory_manager_->GetAvailableVRAM();
    uint64_t reserved = memory_manager_->GetReservedBuffer();
    uint64_t usable = memory_manager_->GetUsableVRAM();

    float total_gb = total_vram / (1024.0*1024.0*1024.0);
    float available_gb = available_vram / (1024.0*1024.0*1024.0);
    float reserved_gb = reserved / (1024.0*1024.0*1024.0);
    float usable_gb = usable / (1024.0*1024.0*1024.0);
    float utilization = memory_manager_->GetVRAMUtilizationPercent();

    ImGui::Columns(2, "##vram_cols", true);

    ImGui::Text("Total VRAM:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f GB", total_gb);
    ImGui::NextColumn();

    ImGui::Text("Utilization:");
    ImGui::NextColumn();
    ImVec4 util_color(0.2f, 0.8f, 0.2f, 1.0f);
    if (utilization > 90.0f) util_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    else if (utilization > 75.0f) util_color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    ImGui::TextColored(util_color, "%.1f%%", utilization);
    ImGui::NextColumn();

    ImGui::Text("Reserved Buffer:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.2f GB", reserved_gb);
    ImGui::NextColumn();

    ImGui::Text("Usable for Models:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "%.2f GB", usable_gb);
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Separator();

    // TPS Metrics
    ImGui::Text("⚡ TPS METRICS:");

    auto tps_metrics = memory_manager_->GetTPSMetrics();

    ImGui::Columns(2, "##tps_cols", true);

    ImGui::Text("Current TPS:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.2f", tps_metrics.current_tps);
    ImGui::NextColumn();

    ImGui::Text("Average TPS:");
    ImGui::NextColumn();
    ImGui::Text("%.2f", tps_metrics.average_tps);
    ImGui::NextColumn();

    ImGui::Text("Memory Overhead:");
    ImGui::NextColumn();
    ImGui::Text("%.1f%%", tps_metrics.memory_overhead_percent);
    ImGui::NextColumn();

    ImGui::Text("Is Optimal:");
    ImGui::NextColumn();
    ImGui::TextColored(tps_metrics.is_optimal ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                      tps_metrics.is_optimal ? "✓ YES" : "⚠️ NO");
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::ProgressBar(utilization / 100.0f, ImVec2(-1, 30));
    ImGui::SameLine();
    ImGui::Text("VRAM Used");

    if (ImGui::Button("🔄 REFRESH STATUS", ImVec2(-1, 40))) {
        UpdateHistories();
    }

    if (ImGui::Button("🧹 OPTIMIZE MEMORY", ImVec2(-1, 40))) {
        if (memory_manager_) {
            memory_manager_->OptimizeForThroughput();
        }
    }
}

void GUIMemoryBufferPanel::UpdateHistories() {
    if (!memory_manager_) return;

    uint64_t total_vram = memory_manager_->GetTotalVRAM();
    uint64_t available_vram = memory_manager_->GetAvailableVRAM();
    float vram_util = (total_vram > 0) ? ((total_vram - available_vram) / static_cast<float>(total_vram)) * 100.0f : 0.0f;

    vram_history_.erase(vram_history_.begin());
    vram_history_.push_back(vram_util);

    uint64_t total_ram = memory_manager_->GetTotalSystemRAM();
    uint64_t available_ram = memory_manager_->GetAvailableSystemRAM();
    float ram_mb = (total_ram - available_ram) / (1024.0*1024.0);

    ram_history_.erase(ram_history_.begin());
    ram_history_.push_back(ram_mb);
}
void GUIMemoryBufferPanel::Render() {
    // Top-level entry point for the panel; main_visual.cpp wires this into
    // the OmniInference window. The two subsections below are the only ones
    // currently implemented — additional `Render*` slots are reserved in
    // the header for future expansion (allocation table, tiering, etc.).
    if (ImGui::Begin("Memory & Buffer")) {
        if (ImGui::CollapsingHeader("Buffer Configuration",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderBufferConfiguration();
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("VRAM Status",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderVRAMStatus();
        }
    }
    ImGui::End();
}
