#include "turboquant_pipeline.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <chrono>

#ifdef __CUDACC__
    #include <cuda_runtime.h>
    extern "C" {
        void launch_polar_transform_cuda(float* input, float* mag, float* phase, int n, cudaStream_t stream);
        void launch_quantize_cuda(float* input, uint8_t* output, int n, int bits, float scale, cudaStream_t stream);
        void launch_dequantize_cuda(uint8_t* input, float* output, int n, int bits, float scale, cudaStream_t stream);
        void launch_qjl_correction_cuda(float* data, float* residuals, int n, float weight, cudaStream_t stream);
    }
#endif

TurboQuantPipeline::TurboQuantPipeline() = default;

TurboQuantPipeline::~TurboQuantPipeline() = default;

void TurboQuantPipeline::AddPipelineNode(const QuantPipelineNode& node) {
    pipeline_.push_back(node);
    std::cout << "[TurboQuant Pipeline] Added node: " << node.node_id 
              << " (Stage: " << static_cast<int>(node.stage) << ")" << std::endl;
}

void TurboQuantPipeline::RemovePipelineNode(const std::string& node_id) {
    pipeline_.erase(
        std::remove_if(pipeline_.begin(), pipeline_.end(),
            [&node_id](const QuantPipelineNode& n) { return n.node_id == node_id; }),
        pipeline_.end()
    );
}

void TurboQuantPipeline::ClearPipeline() {
    pipeline_.clear();
}

std::vector<QuantPipelineNode> TurboQuantPipeline::GetPipeline() const {
    return pipeline_;
}

void TurboQuantPipeline::PolarTransform(
    const std::vector<float>& input_kv,
    std::vector<float>& magnitude,
    std::vector<float>& phase) {

    magnitude.resize(input_kv.size() / 2);
    phase.resize(input_kv.size() / 2);

    if (use_cuda_) {
        #ifdef __CUDACC__
        float* d_input = nullptr;
        float* d_magnitude = nullptr;
        float* d_phase = nullptr;

        cudaMalloc(&d_input, input_kv.size() * sizeof(float));
        cudaMalloc(&d_magnitude, magnitude.size() * sizeof(float));
        cudaMalloc(&d_phase, phase.size() * sizeof(float));

        cudaMemcpy(d_input, input_kv.data(), input_kv.size() * sizeof(float), cudaMemcpyHostToDevice);

        launch_polar_transform_cuda(d_input, d_magnitude, d_phase, input_kv.size() / 2, 
                                    static_cast<cudaStream_t>(cuda_stream_));

        cudaMemcpy(magnitude.data(), d_magnitude, magnitude.size() * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(phase.data(), d_phase, phase.size() * sizeof(float), cudaMemcpyDeviceToHost);

        cudaFree(d_input);
        cudaFree(d_magnitude);
        cudaFree(d_phase);
        #endif
    } else {
        // CPU implementation
        for (size_t i = 0; i < input_kv.size(); i += 2) {
            float x = input_kv[i];
            float y = (i + 1 < input_kv.size()) ? input_kv[i + 1] : 0.0f;

            magnitude[i / 2] = std::sqrt(x * x + y * y);
            phase[i / 2] = std::atan2(y, x);
        }
    }

    if (progress_callback_) {
        progress_callback_(0.15f, "Polar transform complete");
    }
}

void TurboQuantPipeline::InversePolarTransform(
    const std::vector<float>& magnitude,
    const std::vector<float>& phase,
    std::vector<float>& output) {

    output.resize(magnitude.size() * 2);

    for (size_t i = 0; i < magnitude.size(); ++i) {
        float r = magnitude[i];
        float theta = phase[i];

        output[2 * i] = r * std::cos(theta);
        output[2 * i + 1] = r * std::sin(theta);
    }
}

void TurboQuantPipeline::QuantizeMagnitude(
    const std::vector<float>& magnitude,
    std::vector<uint8_t>& quantized,
    int bit_width,
    float scale) {

    quantized.resize((magnitude.size() * bit_width + 7) / 8);
    std::fill(quantized.begin(), quantized.end(), 0);

    int max_val = (1 << bit_width) - 1;

    if (use_cuda_) {
        #ifdef __CUDACC__
        float* d_magnitude = nullptr;
        uint8_t* d_quantized = nullptr;

        cudaMalloc(&d_magnitude, magnitude.size() * sizeof(float));
        cudaMalloc(&d_quantized, quantized.size());

        cudaMemcpy(d_magnitude, magnitude.data(), magnitude.size() * sizeof(float), cudaMemcpyHostToDevice);

        launch_quantize_cuda(d_magnitude, d_quantized, magnitude.size(), bit_width, scale,
                            static_cast<cudaStream_t>(cuda_stream_));

        cudaMemcpy(quantized.data(), d_quantized, quantized.size(), cudaMemcpyDeviceToHost);

        cudaFree(d_magnitude);
        cudaFree(d_quantized);
        #endif
    } else {
        // CPU implementation with bit-packing
        for (size_t i = 0; i < magnitude.size(); ++i) {
            float normalized = (magnitude[i] / scale);
            normalized = std::max(0.0f, std::min(1.0f, normalized));

            uint8_t quantized_val = static_cast<uint8_t>(normalized * max_val);

            int bit_pos = i * bit_width;
            int byte_pos = bit_pos / 8;
            int bit_offset = bit_pos % 8;

            if (bit_width <= 8 - bit_offset) {
                quantized[byte_pos] |= (quantized_val << bit_offset);
            } else {
                int remaining_bits = 8 - bit_offset;
                quantized[byte_pos] |= ((quantized_val & ((1 << remaining_bits) - 1)) << bit_offset);
                quantized[byte_pos + 1] |= (quantized_val >> remaining_bits);
            }
        }
    }

    if (progress_callback_) {
        progress_callback_(0.35f, "Magnitude quantization complete");
    }
}

void TurboQuantPipeline::DequantizeMagnitude(
    const std::vector<uint8_t>& quantized,
    std::vector<float>& magnitude,
    int bit_width,
    float scale) {

    if (use_cuda_) {
        #ifdef __CUDACC__
        uint8_t* d_quantized = nullptr;
        float* d_magnitude = nullptr;

        cudaMalloc(&d_quantized, quantized.size());
        cudaMalloc(&d_magnitude, magnitude.size() * sizeof(float));

        cudaMemcpy(d_quantized, quantized.data(), quantized.size(), cudaMemcpyHostToDevice);

        launch_dequantize_cuda(d_quantized, d_magnitude, magnitude.size(), bit_width, scale,
                              static_cast<cudaStream_t>(cuda_stream_));

        cudaMemcpy(magnitude.data(), d_magnitude, magnitude.size() * sizeof(float), cudaMemcpyDeviceToHost);

        cudaFree(d_quantized);
        cudaFree(d_magnitude);
        #endif
    } else {
        // CPU implementation
        int max_val = (1 << bit_width) - 1;

        for (size_t i = 0; i < magnitude.size(); ++i) {
            int bit_pos = i * bit_width;
            int byte_pos = bit_pos / 8;
            int bit_offset = bit_pos % 8;

            uint8_t quantized_val = 0;

            if (bit_width <= 8 - bit_offset) {
                quantized_val = (quantized[byte_pos] >> bit_offset) & ((1 << bit_width) - 1);
            } else {
                int remaining_bits = 8 - bit_offset;
                quantized_val = (quantized[byte_pos] >> bit_offset) & ((1 << remaining_bits) - 1);
                quantized_val |= ((quantized[byte_pos + 1] & ((1 << (bit_width - remaining_bits)) - 1)) << remaining_bits);
            }

            magnitude[i] = (static_cast<float>(quantized_val) / max_val) * scale;
        }
    }
}

void TurboQuantPipeline::QuantizePhase(
    const std::vector<float>& phase,
    std::vector<uint8_t>& quantized,
    int bit_width) {

    quantized.resize((phase.size() * bit_width + 7) / 8);
    std::fill(quantized.begin(), quantized.end(), 0);

    int max_val = (1 << bit_width) - 1;
    float phase_range = 2.0f * 3.14159265359f;

    for (size_t i = 0; i < phase.size(); ++i) {
        float normalized = (phase[i] + 3.14159265359f) / phase_range;
        normalized = std::max(0.0f, std::min(1.0f, normalized));

        uint8_t quantized_val = static_cast<uint8_t>(normalized * max_val);

        int bit_pos = i * bit_width;
        int byte_pos = bit_pos / 8;
        int bit_offset = bit_pos % 8;

        if (bit_width <= 8 - bit_offset) {
            quantized[byte_pos] |= (quantized_val << bit_offset);
        } else {
            int remaining_bits = 8 - bit_offset;
            quantized[byte_pos] |= ((quantized_val & ((1 << remaining_bits) - 1)) << bit_offset);
            quantized[byte_pos + 1] |= (quantized_val >> remaining_bits);
        }
    }

    if (progress_callback_) {
        progress_callback_(0.55f, "Phase quantization complete");
    }
}

void TurboQuantPipeline::DequantizePhase(
    const std::vector<uint8_t>& quantized,
    std::vector<float>& phase,
    int bit_width) {

    int max_val = (1 << bit_width) - 1;
    float phase_range = 2.0f * 3.14159265359f;

    for (size_t i = 0; i < phase.size(); ++i) {
        int bit_pos = i * bit_width;
        int byte_pos = bit_pos / 8;
        int bit_offset = bit_pos % 8;

        uint8_t quantized_val = 0;

        if (bit_width <= 8 - bit_offset) {
            quantized_val = (quantized[byte_pos] >> bit_offset) & ((1 << bit_width) - 1);
        } else {
            int remaining_bits = 8 - bit_offset;
            quantized_val = (quantized[byte_pos] >> bit_offset) & ((1 << remaining_bits) - 1);
            quantized_val |= ((quantized[byte_pos + 1] & ((1 << (bit_width - remaining_bits)) - 1)) << remaining_bits);
        }

        phase[i] = (static_cast<float>(quantized_val) / max_val) * phase_range - 3.14159265359f;
    }
}

void TurboQuantPipeline::ComputeQJLResiduals(
    const std::vector<float>& original,
    const std::vector<float>& reconstructed,
    std::vector<float>& residuals) {

    residuals.resize(original.size());

    for (size_t i = 0; i < original.size(); ++i) {
        residuals[i] = original[i] - reconstructed[i];
    }

    if (progress_callback_) {
        progress_callback_(0.75f, "QJL residuals computed");
    }
}

void TurboQuantPipeline::ApplyQJLCorrection(
    std::vector<float>& reconstructed,
    const std::vector<float>& residuals,
    float correction_weight) {

    for (size_t i = 0; i < reconstructed.size(); ++i) {
        reconstructed[i] += residuals[i] * correction_weight;
    }

    if (progress_callback_) {
        progress_callback_(0.90f, "QJL correction applied");
    }
}

TurboQuantState TurboQuantPipeline::ExecutePipeline(
    const std::vector<float>& input_kv,
    int key_bits,
    int value_bits,
    bool use_qjl) {

    auto start_time = std::chrono::high_resolution_clock::now();

    TurboQuantState state;

    // Stage 1: Polar Transform
    std::cout << "[TurboQuant Pipeline] Stage 1: Polar Transform" << std::endl;
    PolarTransform(input_kv, state.polar_magnitude, state.polar_phase);

    // Stage 2: Magnitude Quantization (Turbo-K)
    std::cout << "[TurboQuant Pipeline] Stage 2: Magnitude Quantization (" << key_bits << "-bit)" << std::endl;
    QuantizeMagnitude(state.polar_magnitude, state.quantized_magnitude, key_bits, 10.0f);
    DequantizeMagnitude(state.quantized_magnitude, state.polar_magnitude, key_bits, 10.0f);

    // Stage 3: Phase Quantization (Turbo-V)
    std::cout << "[TurboQuant Pipeline] Stage 3: Phase Quantization (" << value_bits << "-bit)" << std::endl;
    QuantizePhase(state.polar_phase, state.quantized_phase, value_bits);
    DequantizePhase(state.quantized_phase, state.polar_phase, value_bits);

    // Stage 4: Inverse Polar Transform
    std::cout << "[TurboQuant Pipeline] Stage 4: Inverse Polar Transform" << std::endl;
    InversePolarTransform(state.polar_magnitude, state.polar_phase, state.reconstructed_values);

    // Stage 5: QJL Correction (Optional)
    if (use_qjl) {
        std::cout << "[TurboQuant Pipeline] Stage 5: QJL Residual Correction" << std::endl;
        ComputeQJLResiduals(input_kv, state.reconstructed_values, state.qjl_residuals);
        ApplyQJLCorrection(state.reconstructed_values, state.qjl_residuals, 0.5f);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "[TurboQuant Pipeline] Pipeline execution completed in " << duration.count() << "ms" << std::endl;

    if (progress_callback_) {
        progress_callback_(1.0f, "Pipeline execution complete");
    }

    return state;
}

std::vector<float> TurboQuantPipeline::ReconstructFromPipeline(const TurboQuantState& state) {
    return state.reconstructed_values;
}

QuantizationMetrics TurboQuantPipeline::ComputeMetrics(
    const std::vector<float>& original,
    const std::vector<float>& reconstructed,
    size_t original_size_bytes) {

    QuantizationMetrics metrics;

    // Calculate reconstruction error
    float sum_squared_error = 0.0f;
    for (size_t i = 0; i < original.size(); ++i) {
        float error = original[i] - reconstructed[i];
        sum_squared_error += error * error;
    }
    metrics.reconstruction_error = std::sqrt(sum_squared_error / original.size());

    // Compression ratio
    size_t quantized_size = (original.size() * (4 + 2) / 8); // Key + Value bits
    metrics.compression_ratio = static_cast<float>(original_size_bytes) / quantized_size;
    metrics.memory_saved_mb = (original_size_bytes - quantized_size) / (1024.0f * 1024.0f);

    return metrics;
}

bool TurboQuantPipeline::NeedleInHaystackTest(
    const std::vector<float>& original_values,
    const TurboQuantState& quantized_state,
    float similarity_threshold) {

    // Test if we can still find the "needle" (important value) in the "haystack" (KV cache)
    float max_original = *std::max_element(original_values.begin(), original_values.end());
    float max_reconstructed = *std::max_element(quantized_state.reconstructed_values.begin(), 
                                                quantized_state.reconstructed_values.end());

    float needle_similarity = max_reconstructed / max_original;

    std::cout << "[NeedleInHaystack] Max original: " << max_original << std::endl;
    std::cout << "[NeedleInHaystack] Max reconstructed: " << max_reconstructed << std::endl;
    std::cout << "[NeedleInHaystack] Similarity: " << needle_similarity << std::endl;

    return needle_similarity >= similarity_threshold;
}

bool TurboQuantPipeline::UseCUDAAcceleration(bool use_cuda) {
    use_cuda_ = use_cuda;
    std::cout << "[TurboQuant Pipeline] CUDA acceleration: " << (use_cuda ? "ENABLED" : "DISABLED") << std::endl;
    return use_cuda_;
}

void TurboQuantPipeline::SetCUDAStream(void* stream) {
    cuda_stream_ = stream;
}

json TurboQuantPipeline::ExportPipelineJSON() const {
    json config = json::array();

    for (const auto& node : pipeline_) {
        json node_json;
        node_json["node_id"] = node.node_id;
        node_json["stage"] = static_cast<int>(node.stage);
        node_json["enabled"] = node.enabled;
        node_json["parameters"] = node.parameters;
        config.push_back(node_json);
    }

    return config;
}

void TurboQuantPipeline::ImportPipelineJSON(const json& config) {
    pipeline_.clear();

    for (const auto& node_json : config) {
        QuantPipelineNode node;
        node.node_id = node_json.value("node_id", "");
        node.stage = static_cast<QuantPipelineStage>(node_json.value("stage", 0));
        node.enabled = node_json.value("enabled", true);
        node.parameters = node_json.value("parameters", std::map<std::string, float>{});

        AddPipelineNode(node);
    }
}

void TurboQuantPipeline::SetProgressCallback(ProgressCallback callback) {
    progress_callback_ = callback;
}

void TurboQuantPipeline::LaunchPolarTransformKernel(const float* input, float* magnitude, float* phase, int n) {
    // CUDA kernel launch wrapper
}

void TurboQuantPipeline::LaunchQuantizeKernel(const float* input, uint8_t* output, int n, int bits, float scale) {
    // CUDA kernel launch wrapper
}

void TurboQuantPipeline::LaunchDequantizeKernel(const uint8_t* input, float* output, int n, int bits, float scale) {
    // CUDA kernel launch wrapper
}

void TurboQuantPipeline::LaunchQJLCorrectionKernel(float* data, const float* residuals, int n, float weight) {
    // CUDA kernel launch wrapper
}