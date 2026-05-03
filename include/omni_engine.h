#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <cstdint>

// Forward declarations
struct cudaStream_t;
struct VkDevice_T;

enum class HardwareBackend {
    NVIDIA_CUDA,
    AMD_VULKAN,
    INTEL_VULKAN,
    INTEL_SYCL,
    CPU_FALLBACK
};

enum class QuantizationMode {
    NONE,
    TURBO_QUANT_2BIT,
    TURBO_QUANT_3BIT,
    TURBO_QUANT_4BIT,
    TURBO_QUANT_6BIT,
    TURBO_QUANT_8BIT
};

struct TurboQuantConfig {
    QuantizationMode mode = QuantizationMode::TURBO_QUANT_3BIT;
    int key_bits = 4;
    int value_bits = 2;
    bool use_polar_transform = true;
    bool use_qjl_correction = true;
    float polar_scale = 1.0f;
    bool enable_block_sparse = false;
    int block_size = 64;
};

struct HardwareProfile {
    HardwareBackend backend;
    std::string device_name;
    uint64_t vram_total_bytes;
    uint64_t vram_free_bytes;
    uint32_t compute_capability; // For CUDA: major*10 + minor
    int cuda_capability_major;
    int cuda_capability_minor;
    std::string cuda_version;
    std::string driver_version;
    bool supports_fp16;
    bool supports_tensor_cores;
    bool supports_flash_attention;
    bool supports_rope_freq_scaling;
};

struct ModelParameters {
    std::string model_path;
    std::string model_name;
    int n_embd = 4096;
    int n_layer = 32;
    int n_head = 32;
    int n_head_kv = 8;
    int n_vocab = 128000;
    int n_ctx = 4096;
    int n_batch = 512;
    float rope_freq_base = 500000.0f;
    float rope_freq_scale = 1.0f;
    int n_gpu_layers = -1; // -1 = auto
    bool use_mmap = true;
    bool use_mlock = false;
    bool flash_attn = false;
    int threads = 4;
};

struct GenerationConfig {
    float temperature = 0.7f;
    float top_p = 0.95f;
    float top_k = 40.0f;
    float min_p = 0.0f;
    int max_tokens = 512;
    bool use_attention_gated_delta = false;
    int sparse_threshold = 0; // 0 = disabled
};

struct PipelineNode {
    std::string node_id;
    std::string node_type; // "input", "model", "quantize", "decode", "output"
    std::map<std::string, float> parameters;
    std::vector<std::string> input_nodes;
    bool enabled = true;
};

struct PipelineConfig {
    std::vector<PipelineNode> nodes;
    std::map<std::string, std::string> connections;
};

class OmniEngine {
public:
    OmniEngine();
    ~OmniEngine();

    // Hardware Detection & Initialization
    HardwareProfile DetectHardware();
    bool InitializeBackend(HardwareBackend preferred = HardwareBackend::NVIDIA_CUDA);
    HardwareBackend GetActiveBackend() const { return active_backend_; }
    const HardwareProfile& GetHardwareProfile() const { return hw_profile_; }

    // Model Management
    bool LoadModel(const ModelParameters& params, const TurboQuantConfig& quant_cfg);
    bool UnloadModel();
    bool IsModelLoaded() const { return model_loaded_; }

    // Quantization Control
    bool ApplyQuantization(const TurboQuantConfig& config);
    TurboQuantConfig GetCurrentQuantConfig() const { return active_quant_cfg_; }
    bool ValidateQuantizationFit(const ModelParameters& params, const TurboQuantConfig& config);

    // Generation
    std::string Generate(
        const std::string& prompt,
        const GenerationConfig& gen_cfg,
        std::function<void(const std::string&)> token_callback = nullptr
    );

    // Pipeline Management
    bool CreatePipeline(const PipelineConfig& pipeline);
    bool ExecutePipeline();
    PipelineConfig GetCurrentPipeline() const { return current_pipeline_; }
    void SetPipelineNode(const PipelineNode& node);

    // Parameter Control
    void SetModelParameter(const std::string& param_name, float value);
    float GetModelParameter(const std::string& param_name) const;
    std::map<std::string, float> GetAllModelParameters() const;

    // Memory & Resource Optimization
    uint64_t EstimateMemoryUsage(const ModelParameters& params, const TurboQuantConfig& quant) const;
    bool OptimizeForHardware(ModelParameters& params, TurboQuantConfig& quant);
    void PrintHardwareReport() const;

    // Callback hooks
    void SetTokenCallback(std::function<void(const std::string&)> callback) {
        token_callback_ = callback;
    }

private:
    HardwareBackend active_backend_ = HardwareBackend::CPU_FALLBACK;
    HardwareProfile hw_profile_;
    bool model_loaded_ = false;
    ModelParameters current_model_params_;
    TurboQuantConfig active_quant_cfg_;
    PipelineConfig current_pipeline_;
    std::function<void(const std::string&)> token_callback_;

    // Backend-specific pointers
    void* cuda_context_ = nullptr;
    void* vulkan_context_ = nullptr;

    // Internal methods
    void InitializeCUDABackend();
    void InitializeVulkanBackend();
    void ApplyOptimizationMatrix();
};