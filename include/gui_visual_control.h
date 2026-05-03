#pragma once

#include "omni_engine.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

struct VisualControlPanel {
    bool expanded = true;
    ImVec2 window_size;
    ImVec2 window_pos;
};

struct SliderControl {
    std::string label;
    float* value;
    float min_val;
    float max_val;
    const char* format;
};

struct ComboControl {
    std::string label;
    int* current;
    std::vector<std::string> options;
};

struct CheckboxControl {
    std::string label;
    bool* value;
};

struct ButtonControl {
    std::string label;
    std::function<void()> callback;
    ImVec4 color;
};

class GUIVisualControl {
public:
    GUIVisualControl();
    ~GUIVisualControl() = default;

    // Main Rendering
    void RenderFullControlPanel();

    // Individual Control Panels
    void RenderHardwareSelectionPanel();
    void RenderModelLoadingPanel();
    void RenderQuantizationControlPanel();
    void RenderInferenceParametersPanel();
    void RenderGenerationParametersPanel();
    void RenderPipelineVisualizerPanel();
    void RenderMemoryOptimizerPanel();
    void RenderLiveMonitoringPanel();

    // Low-level control elements
    void RenderSlider(const SliderControl& ctrl);
    void RenderCombo(const ComboControl& ctrl);
    void RenderCheckbox(const CheckboxControl& ctrl);
    void RenderButton(const ButtonControl& ctrl);
    void RenderColoredButton(const std::string& label, ImVec4 color, std::function<void()> callback);
    void RenderToggleButton(const std::string& label, bool& state);
    void RenderProgressBar(const std::string& label, float progress, const ImVec2& size = ImVec2(-1, 20));

    // State management
    void SetEngine(OmniEngine* engine) { engine_ = engine; }
    ModelParameters GetCurrentModelParams() const { return current_model_params_; }
    GenerationConfig GetCurrentGenConfig() const { return current_gen_config_; }
    TurboQuantConfig GetCurrentQuantConfig() const { return current_quant_config_; }

    // Visual Indicators
    void RenderHardwareIndicator();
    void RenderMemoryUsageGraph();
    void RenderTokenSpeedometer();
    void RenderQuantizationQualityMeter();

private:
    OmniEngine* engine_ = nullptr;

    // Current state
    ModelParameters current_model_params_;
    GenerationConfig current_gen_config_;
    TurboQuantConfig current_quant_config_;

    // UI State
    VisualControlPanel hardware_panel_;
    VisualControlPanel model_panel_;
    VisualControlPanel quantization_panel_;
    VisualControlPanel inference_panel_;
    VisualControlPanel generation_panel_;
    VisualControlPanel pipeline_panel_;
    VisualControlPanel memory_panel_;
    VisualControlPanel monitoring_panel_;

    // Data for visualization
    std::vector<float> memory_history_;
    std::vector<float> speed_history_;
    float current_tps_ = 0.0f;
    float current_memory_usage_mb_ = 0.0f;
    int selected_backend_ = 0;
    int selected_model_ = -1;
    std::vector<std::string> available_models_;

    // Helper rendering functions
    void RenderPanelHeader(const std::string& title, bool& expanded);
    void RenderInfoBox(const std::string& label, const std::string& value, ImVec4 color);
    void RenderStatusIndicator(const std::string& label, bool active, ImVec4 color);
    void UpdateMemoryHistory();
    void UpdateSpeedHistory();
};