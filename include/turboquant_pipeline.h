#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
using nlohmann::json;
#include <functional>
#include <memory>

using json = nlohmann::json;

// ============================================================
// TURBOQUANT PLUS NATIVE PIPELINE SYSTEM
// Full Control: Quantization, Dequantization, Optimization
// ============================================================

enum class QuantPipelineStage {
    INPUT,
    POLAR_TRANSFORM,
    MAGNITUDE_QUANTIZE,
    PHASE_QUANTIZE,
    QJL_RESIDUAL,
    DEQUANTIZE,
    OUTPUT
};

struct QuantPipelineNode {
    std::string node_id;
    QuantPipelineStage stage;
    std::map<std::string, float> parameters;
    bool enabled = true;
    float processing_time_ms = 0.0f;
};

struct QuantizationMetrics {
    float compression_ratio = 0.0f;
    float reconstruction_error = 0.0f;
    float memory_saved_mb = 0.0f;
    float throughput_tokens_per_second = 0.0f;
    float latency_ms = 0.0f;
    bool needle_in_haystack_test_passed = false;
};

struct TurboQuantState {
    std::vector<float> polar_magnitude;
    std::vector<float> polar_phase;
    std::vector<uint8_t> quantized_magnitude;
    std::vector<uint8_t> quantized_phase;
    std::vector<float> qjl_residuals;
    std::vector<float> reconstructed_values;
};

class TurboQuantPipeline {
public:
    TurboQuantPipeline();
    ~TurboQuantPipeline();

    // ========== PIPELINE CONSTRUCTION ==========
    void AddPipelineNode(const QuantPipelineNode& node);
    void RemovePipelineNode(const std::string& node_id);
    void ClearPipeline();
    std::vector<QuantPipelineNode> GetPipeline() const;

    // ========== POLAR QUANTIZATION ==========
    void PolarTransform(
        const std::vector<float>& input_kv,
        std::vector<float>& magnitude,
        std::vector<float>& phase
    );

    void InversePolarTransform(
        const std::vector<float>& magnitude,
        const std::vector<float>& phase,
        std::vector<float>& output
    );

    // ========== MAGNITUDE QUANTIZATION (Turbo-K) ==========
    void QuantizeMagnitude(
        const std::vector<float>& magnitude,
        std::vector<uint8_t>& quantized,
        int bit_width = 4,
        float scale = 10.0f
    );

    void DequantizeMagnitude(
        const std::vector<uint8_t>& quantized,
        std::vector<float>& magnitude,
        int bit_width = 4,
        float scale = 10.0f
    );

    // ========== PHASE QUANTIZATION (Turbo-V) ==========
    void QuantizePhase(
        const std::vector<float>& phase,
        std::vector<uint8_t>& quantized,
        int bit_width = 2
    );

    void DequantizePhase(
        const std::vector<uint8_t>& quantized,
        std::vector<float>& phase,
        int bit_width = 2
    );

    // ========== QJL CORRECTION ==========
    void ComputeQJLResiduals(
        const std::vector<float>& original,
        const std::vector<float>& reconstructed,
        std::vector<float>& residuals
    );

    void ApplyQJLCorrection(
        std::vector<float>& reconstructed,
        const std::vector<float>& residuals,
        float correction_weight = 1.0f
    );

    // ========== FULL PIPELINE EXECUTION ==========
    TurboQuantState ExecutePipeline(
        const std::vector<float>& input_kv,
        int key_bits = 4,
        int value_bits = 2,
        bool use_qjl = true
    );

    std::vector<float> ReconstructFromPipeline(const TurboQuantState& state);

    // ========== METRICS & TESTING ==========
    QuantizationMetrics ComputeMetrics(
        const std::vector<float>& original,
        const std::vector<float>& reconstructed,
        size_t original_size_bytes
    );

    bool NeedleInHaystackTest(
        const std::vector<float>& original_values,
        const TurboQuantState& quantized_state,
        float similarity_threshold = 0.95f
    );

    // ========== CUDA KERNEL INTEGRATION ==========
    bool UseCUDAAcceleration(bool use_cuda);
    void SetCUDAStream(void* stream);

    // ========== VISUALIZATION ==========
    json ExportPipelineJSON() const;
    void ImportPipelineJSON(const json& config);

    // ========== CALLBACKS ==========
    using ProgressCallback = std::function<void(float, const std::string&)>;
    void SetProgressCallback(ProgressCallback callback);

private:
    std::vector<QuantPipelineNode> pipeline_;
    TurboQuantState current_state_;
    bool use_cuda_ = false;
    void* cuda_stream_ = nullptr;
    ProgressCallback progress_callback_;

    // CUDA kernel declarations
    void LaunchPolarTransformKernel(const float* input, float* magnitude, float* phase, int n);
    void LaunchQuantizeKernel(const float* input, uint8_t* output, int n, int bits, float scale);
    void LaunchDequantizeKernel(const uint8_t* input, float* output, int n, int bits, float scale);
    void LaunchQJLCorrectionKernel(float* data, const float* residuals, int n, float weight);
};