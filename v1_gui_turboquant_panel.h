#pragma once

#include "turboquant_pipeline.h"
#include <imgui.h>
#include <vector>
#include <string>

class GUITurboQuantPanel {
public:
    GUITurboQuantPanel();
    ~GUITurboQuantPanel() = default;

    void Render();
    void SetPipeline(TurboQuantPipeline* pipeline) { pipeline_ = pipeline; }

private:
    TurboQuantPipeline* pipeline_ = nullptr;

    // UI State
    bool show_pipeline_editor_ = true;
    bool show_metrics_ = true;
    bool show_needle_test_ = false;
    int selected_node_ = -1;

    // Visualization
    std::vector<float> compression_history_;
    std::vector<float> error_history_;
    std::vector<float> throughput_history_;

    // Rendering methods
    void RenderPipelineEditor();
    void RenderPipelineVisualizer();
    void RenderMetricsPanel();
    void RenderNeedleInHaystackTest();
    void RenderAdvancedSettings();
    void RenderPresets();

    // Helper functions
    void AddPipelineStage(QuantPipelineStage stage);
    void RemoveSelectedNode();
    void UpdateMetricsHistory();
};