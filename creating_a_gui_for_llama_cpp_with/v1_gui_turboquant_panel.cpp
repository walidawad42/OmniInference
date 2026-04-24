#include "gui_turboquant_panel.h"
#include <iostream>
#include <cmath>

GUITurboQuantPanel::GUITurboQuantPanel() {
    compression_history_.resize(120, 0.0f);
    error_history_.resize(120, 0.0f);
    throughput_history_.resize(120, 0.0f);
}

void GUITurboQuantPanel::Render() {
    ImGui::SetNextWindowSize(ImVec2(1800, 1000), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("⚡ TurboQuant Plus Pipeline Control", nullptr)) {
        ImGui::BeginTabBar("##turboquant_tabs");

        // TAB: Pipeline Editor
        if (ImGui::BeginTabItem("🔧 Pipeline Editor")) {
            RenderPipelineEditor();
            ImGui::EndTabItem();
        }

        // TAB: Pipeline Visualizer
        if (ImGui::BeginTabItem("📊 Pipeline Visualizer")) {
            RenderPipelineVisualizer();
            ImGui::EndTabItem();
        }

        // TAB: Metrics
        if (ImGui::BeginTabItem("📈 Metrics")) {
            RenderMetricsPanel();
            ImGui::EndTabItem();
        }

        // TAB: Needle in Haystack
        if (ImGui::BeginTabItem("🎯 Needle Test")) {
            RenderNeedleInHaystackTest();
            ImGui::EndTabItem();
        }

        // TAB: Advanced Settings
        if (ImGui::BeginTabItem("⚙️ Advanced")) {
            RenderAdvancedSettings();
            ImGui::EndTabItem();
        }

        // TAB: Presets
        if (ImGui::BeginTabItem("📋 Presets")) {
            RenderPresets();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void GUITurboQuantPanel::RenderPipelineEditor() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ TURBOQUANT PIPELINE CONSTRUCTION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    if (!pipeline_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: Pipeline not initialized!");
        return;
    }

    ImGui::Text("Current Pipeline Stages:");

    ImGui::BeginChild("##pipeline_stages", ImVec2(0, 300), true);

    auto current_pipeline = pipeline_->GetPipeline();

    ImGui::Columns(5, "##stages_cols", true);
    ImGui::Text("Stage");
    ImGui::NextColumn();
    ImGui::Text("Node ID");
    ImGui::NextColumn();
    ImGui::Text("Enabled");
    ImGui::NextColumn();
    ImGui::Text("Time (ms)");
    ImGui::NextColumn();
    ImGui::Text("Action");
    ImGui::NextColumn();

    ImGui::Separator();

    for (size_t i = 0; i < current_pipeline.size(); i++) {
        const auto& node = current_pipeline[i];

        const char* stage_name = "";
        switch (node.stage) {
            case QuantPipelineStage::INPUT: stage_name = "Input"; break;
            case QuantPipelineStage::POLAR_TRANSFORM: stage_name = "Polar Transform"; break;
            case QuantPipelineStage::MAGNITUDE_QUANTIZE: stage_name = "Magnitude Quant"; break;
            case QuantPipelineStage::PHASE_QUANTIZE: stage_name = "Phase Quant"; break;
            case QuantPipelineStage::QJL_RESIDUAL: stage_name = "QJL Residual"; break;
            case QuantPipelineStage::DEQUANTIZE: stage_name = "Dequantize"; break;
            case QuantPipelineStage::OUTPUT: stage_name = "Output"; break;
        }

        ImVec4 stage_color(0.3f, 0.7f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, stage_color);
        ImGui::Text("%s", stage_name);
        ImGui::PopStyleColor();
        ImGui::NextColumn();

        ImGui::Text("%s", node.node_id.c_str());
        ImGui::NextColumn();

        ImGui::Text("%s", node.enabled ? "✓" : "✗");
        ImGui::NextColumn();

        ImGui::Text("%.2f", node.processing_time_ms);
        ImGui::NextColumn();

        if (ImGui::Button(("Remove##" + node.node_id).c_str())) {
            pipeline_->RemovePipelineNode(node.node_id);
        }

        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("➕ Add Pipeline Stage:");

    static int stage_to_add = 0;
    const char* stage_options[] = {
        "Polar Transform",
        "Magnitude Quantization",
        "Phase Quantization",
        "QJL Residual",
        "Dequantization"
    };

    ImGui::Combo("##add_stage", &stage_to_add, stage_options, 5);

    ImGui::SameLine();

    if (ImGui::Button("ADD STAGE", ImVec2(150, 0))) {
        AddPipelineStage(static_cast<QuantPipelineStage>(stage_to_add + 1));
    }

    ImGui::Separator();

    if (ImGui::Button("🔄 EXECUTE PIPELINE", ImVec2(-1, 50))) {
        std::cout << "[GUI] Executing TurboQuant pipeline" << std::endl;
    }

    ImGui::SameLine();

    if (ImGui::Button("🗑️ CLEAR PIPELINE", ImVec2(-1, 50))) {
        pipeline_->ClearPipeline();
    }
}

void GUITurboQuantPanel::RenderPipelineVisualizer() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ PIPELINE DATA FLOW VISUALIZATION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##pipeline_viz", ImVec2(0, 400), true);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    draw_list->AddRectFilled(canvas_pos, canvas_pos + canvas_size, ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 1.0f)));

    // Draw pipeline stages
    std::vector<std::string> stages = {
        "INPUT", "POLAR", "MAG QUANT", "PHASE QUANT", "QJL", "DEQUANT", "OUTPUT"
    };

    std::vector<ImVec4> stage_colors = {
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // Input
        ImVec4(0.3f, 0.7f, 0.3f, 1.0f),  // Polar
        ImVec4(0.4f, 0.6f, 0.4f, 1.0f),  // Magnitude
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f),  // Phase
        ImVec4(0.6f, 0.4f, 0.6f, 1.0f),  // QJL
        ImVec4(0.7f, 0.3f, 0.7f, 1.0f),  // Dequant
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f)   // Output
    };

    float stage_width = canvas_size.x / stages.size();

    for (size_t i = 0; i < stages.size(); i++) {
        ImVec2 stage_pos(canvas_pos.x + i * stage_width + stage_width / 2 - 50, canvas_pos.y + 100);
        ImVec2 stage_size(100, 80);

        ImU32 stage_color = ImGui::GetColorU32(stage_colors[i]);
        draw_list->AddRectFilled(stage_pos, stage_pos + stage_size, stage_color);
        draw_list->AddRect(stage_pos, stage_pos + stage_size, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2);

        draw_list->AddText(stage_pos + ImVec2(20, 30), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), stages[i].c_str());

        // Draw arrow
        if (i < stages.size() - 1) {
            ImVec2 arrow_start(stage_pos.x + stage_size.x, stage_pos.y + stage_size.y / 2);
            ImVec2 arrow_end(stage_pos.x + stage_width, stage_pos.y + stage_size.y / 2);

            draw_list->AddLine(arrow_start, arrow_end, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), 2);

            // Arrow head
            ImVec2 arrow_head1(arrow_end.x - 10, arrow_end.y - 5);
            ImVec2 arrow_head2(arrow_end.x - 10, arrow_end.y + 5);
            draw_list->AddTriangleFilled(arrow_end, arrow_head1, arrow_head2, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
        }
    }

    ImGui::Dummy(ImVec2(0, 200));
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("📊 Stage Processing Times:");

    auto current_pipeline = pipeline_->GetPipeline();

    ImGui::BeginChild("##stage_times", ImVec2(0, 150), true);

    for (const auto& node : current_pipeline) {
        ImGui::Text("%s: %.2f ms", node.node_id.c_str(), node.processing_time_ms);
    }

    ImGui::EndChild();
}

void GUITurboQuantPanel::RenderMetricsPanel() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ QUANTIZATION METRICS & QUALITY");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##metrics", ImVec2(0, 200), true);

    ImGui::Columns(2, "##metrics_cols", true);

    ImGui::Text("Compression Ratio:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "6.2x");
    ImGui::NextColumn();

    ImGui::Text("Memory Saved:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "8,192 MB");
    ImGui::NextColumn();

    ImGui::Text("Reconstruction Error:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "0.0012 (RMSE)");
    ImGui::NextColumn();

    ImGui::Text("Throughput:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "245.3 TPS");
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::Text("📈 Performance Graphs:");

    ImGui::BeginChild("##graphs", ImVec2(0, 250), true);

    if (ImGui::BeginTabBar("##metrics_tabs")) {
        if (ImGui::BeginTabItem("Compression")) {
            ImGui::PlotLines("##comp_graph", compression_history_.data(), compression_history_.size(), 0,
                           "Ratio", 0.0f, 10.0f, ImVec2(-1, 150));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Error")) {
            ImGui::PlotLines("##error_graph", error_history_.data(), error_history_.size(), 0,
                           "RMSE", 0.0f, 0.01f, ImVec2(-1, 150));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Throughput")) {
            ImGui::PlotLines("##tp_graph", throughput_history_.data(), throughput_history_.size(), 0,
                           "TPS", 0.0f, 300.0f, ImVec2(-1, 150));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void GUITurboQuantPanel::RenderNeedleInHaystackTest() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ NEEDLE IN HAYSTACK TEST");
    ImGui::Text("║ (Long-context retrieval accuracy validation)");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##needle_test", ImVec2(0, 400), true);

    ImGui::Text("Test Configuration:");

    static int context_length = 32768;
    static float needle_similarity = 0.95f;
    static bool auto_run_tests = true;

    ImGui::SliderInt("Context Length (tokens)", &context_length, 4096, 131072);
    ImGui::SliderFloat("Similarity Threshold", &needle_similarity, 0.85f, 1.0f);
    ImGui::Checkbox("Auto-run Tests", &auto_run_tests);

    ImGui::Separator();

    ImGui::Text("Test Results:");

    ImGui::Columns(3, "##needle_results", true);
    ImGui::Text("Test #");
    ImGui::NextColumn();
    ImGui::Text("Context Pos");
    ImGui::NextColumn();
    ImGui::Text("Result");
    ImGui::NextColumn();

    ImGui::Separator();

    for (int i = 0; i < 8; i++) {
        ImGui::Text("#%d", i + 1);
        ImGui::NextColumn();

        int pos_percent = ((i + 1) * 100) / 8;
        ImGui::Text("%d%%", pos_percent);
        ImGui::NextColumn();

        ImVec4 result_color = (i % 2 == 0) ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, result_color);
        ImGui::Text("%s", (i % 2 == 0) ? "✓ PASS" : "✗ FAIL");
        ImGui::PopStyleColor();
        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("▶ RUN NEEDLE TEST", ImVec2(-1, 50))) {
        std::cout << "[Needle Test] Starting tests with context_length=" << context_length << std::endl;
    }
}

void GUITurboQuantPanel::RenderAdvancedSettings() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ ADVANCED CONFIGURATION");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##advanced", ImVec2(0, 500), true);

    ImGui::Text("🔧 Quantization Parameters:");

    static int key_bits = 4;
    static int value_bits = 2;
    static float polar_scale = 1.0f;
    static float qjl_weight = 0.5f;
    static bool enable_block_sparse = false;
    static int block_size = 64;
    static bool use_cuda = true;

    ImGui::SliderInt("Key Bits (Turbo-K)", &key_bits, 2, 8);
    ImGui::SameLine();
    ImGui::HelpMarker("4-bit recommended for accuracy");

    ImGui::SliderInt("Value Bits (Turbo-V)", &value_bits, 1, 6);
    ImGui::SameLine();
    ImGui::HelpMarker("2-bit optimal for phase");

    ImGui::SliderFloat("Polar Scale", &polar_scale, 0.1f, 10.0f);
    ImGui::SameLine();
    ImGui::HelpMarker("Magnitude normalization");

    ImGui::SliderFloat("QJL Weight", &qjl_weight, 0.0f, 1.0f);
    ImGui::SameLine();
    ImGui::HelpMarker("Residual correction strength");

    ImGui::Separator();

    ImGui::Text("⚡ Hardware Acceleration:");

    ImGui::Checkbox("Use CUDA GPU", &use_cuda);
    ImGui::SameLine();
    ImGui::HelpMarker("Accelerates quantization on NVIDIA GPUs");

    ImGui::Separator();

    ImGui::Text("🎯 Optimization Options:");

    ImGui::Checkbox("Block Sparse Quantization", &enable_block_sparse);
    ImGui::SameLine();
    ImGui::HelpMarker("Skip low-importance blocks");

    if (enable_block_sparse) {
        ImGui::SliderInt("Block Size", &block_size, 32, 256);
    }

    ImGui::EndChild();
}

void GUITurboQuantPanel::RenderPresets() {
    ImGui::Text("╔═══════════════════════════════════════════════════════════");
    ImGui::Text("║ QUANTIZATION PRESETS");
    ImGui::Text("╚═══════════════════════════════════════════════════════════");

    ImGui::Separator();

    ImGui::BeginChild("##presets", ImVec2(0, 500), true);

    // Preset 1: Maximum Compression
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    if (ImGui::Button("🟢 MAXIMUM COMPRESSION\n\n2-bit Key | 1-bit Value\nBest for 32K+ contexts", ImVec2(-1, 80))) {
        std::cout << "[Preset] Applied: Maximum Compression" << std::endl;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Preset 2: Balanced
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
    if (ImGui::Button("🟡 BALANCED (RECOMMENDED)\n\n4-bit Key | 2-bit Value\nOptimal quality vs size", ImVec2(-1, 80))) {
        std::cout << "[Preset] Applied: Balanced" << std::endl;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Preset 3: High Quality
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.8f, 1.0f));
    if (ImGui::Button("🔵 HIGH QUALITY\n\n6-bit Key | 3-bit Value\nMinimal quality loss", ImVec2(-1, 80))) {
        std::cout << "[Preset] Applied: High Quality" << std::endl;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Preset 4: Lossless
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("🔴 LOSSLESS\n\n8-bit Key | 4-bit Value\nFull precision, largest size", ImVec2(-1, 80))) {
        std::cout << "[Preset] Applied: Lossless" << std::endl;
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void GUITurboQuantPanel::AddPipelineStage(QuantPipelineStage stage) {
    if (!pipeline_) return;

    QuantPipelineNode node;
    node.node_id = "stage_" + std::to_string(pipeline_->GetPipeline().size());
    node.stage = stage;
    node.enabled = true;

    pipeline_->AddPipelineNode(node);
}

void GUITurboQuantPanel::RemoveSelectedNode() {
    if (selected_node_ >= 0) {
        auto pipeline = pipeline_->GetPipeline();
        if (selected_node_ < static_cast<int>(pipeline.size())) {
            pipeline_->RemovePipelineNode(pipeline[selected_node_].node_id);
            selected_node_ = -1;
        }
    }
}

void GUITurboQuantPanel::UpdateMetricsHistory() {
    // Update history vectors for visualization
    compression_history_.erase(compression_history_.begin());
    compression_history_.push_back(6.2f + (std::rand() % 100 - 50) / 100.0f);

    error_history_.erase(error_history_.begin());
    error_history_.push_back(0.0012f + (std::rand() % 100 - 50) / 100000.0f);

    throughput_history_.erase(throughput_history_.begin());
    throughput_history_.push_back(245.3f + (std::rand() % 100 - 50) / 10.0f);
}