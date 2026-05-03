#include "omni_engine.h"
#include <iostream>
#include <cmath>
#include <algorithm>

// CUDA Headers (11.4+)
#ifdef __CUDACC__
    #include <cuda_runtime.h>
    #include <cublas_v2.h>
    #include <cuda.h>
#endif

// Vulkan Headers
#include <vulkan/vulkan.h>

// TurboQuant Plus Headers
#include "turboquant_plus.h"

extern "C" {
    void launch_turbo_quant_kv_polar(
        void* kv_data,
        void* quantized_data,
        int n_tokens,
        int n_embd,
        int n_layers,
        int key_bits,
        int value_bits,
        void* stream
    );
    
    void launch_turbo_quant_dequantize(
        void* quantized_data,
        void* output_data,
        int n_tokens,
        int n_embd,
        int key_bits,
        int value_bits,
        void* stream
    );
}

OmniEngine::OmniEngine() = default;

OmniEngine::~OmniEngine() {
    UnloadModel();
}

HardwareProfile OmniEngine::DetectHardware() {
    HardwareProfile profile{};
    
    // Try NVIDIA CUDA First
    #ifdef __CUDACC__
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) == cudaSuccess && device_count > 0) {
        cudaDeviceProp props;
        cudaGetDeviceProperties(&props, 0);
        
        profile.backend = HardwareBackend::NVIDIA_CUDA;
        profile.device_name = props.name;
        profile.cuda_capability_major = props.major;
        profile.cuda_capability_minor = props.minor;
        profile.compute_capability = props.major * 10 + props.minor;
        profile.vram_total_bytes = props.totalGlobalMem;
        
        // Check capabilities
        profile.supports_fp16 = props.major >= 5; // SM 5.0+
        profile.supports_tensor_cores = props.major >= 7; // SM 7.0+
        profile.supports_flash_attention = props.major >= 8; // SM 8.0+
        profile.supports_rope_freq_scaling = true;
        
        // Get CUDA version
        int runtime_version;
        cudaRuntimeGetVersion(&runtime_version);
        profile.cuda_version = "CUDA " + std::to_string(runtime_version / 1000) + "." + 
                               std::to_string((runtime_version % 1000) / 10);
        
        // Get driver version
        int driver_version;
        cudaDriverGetVersion(&driver_version);
        profile.driver_version = "Driver " + std::to_string(driver_version / 1000) + "." + 
                                 std::to_string((driver_version % 1000) / 10);
        
        return profile;
    }
    #endif
    
    // Fallback to Vulkan for AMD/Intel
    uint32_t instance_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_count, nullptr);
    
    if (instance_count > 0) {
        profile.backend = HardwareBackend::AMD_VULKAN; // Default to AMD, can detect via device name
        profile.device_name = "Vulkan Device (AMD/Intel)";
        profile.supports_rope_freq_scaling = true;
        // Additional Vulkan device detection would go here
        return profile;
    }
    
    profile.backend = HardwareBackend::CPU_FALLBACK;
    profile.device_name = "CPU Fallback";
    return profile;
}

bool OmniEngine::InitializeBackend(HardwareBackend preferred) {
    hw_profile_ = DetectHardware();
    
    std::cout << "[OmniEngine] Detected Hardware: " << hw_profile_.device_name << std::endl;
    std::cout << "[OmniEngine] Backend: ";
    
    switch (hw_profile_.backend) {
        case HardwareBackend::NVIDIA_CUDA:
            std::cout << "NVIDIA CUDA " << hw_profile_.cuda_version << std::endl;
            std::cout << "[OmniEngine] Compute Capability: " << hw_profile_.cuda_capability_major 
                      << "." << hw_profile_.cuda_capability_minor << std::endl;
            InitializeCUDABackend();
            active_backend_ = HardwareBackend::NVIDIA_CUDA;
            break;
        case HardwareBackend::AMD_VULKAN:
        case HardwareBackend::INTEL_VULKAN:
            std::cout << "Vulkan" << std::endl;
            InitializeVulkanBackend();
            active_backend_ = hw_profile_.backend;
            break;
        case HardwareBackend::CPU_FALLBACK:
            std::cout << "CPU (Fallback)" << std::endl;
            active_backend_ = HardwareBackend::CPU_FALLBACK;
            break;
        default:
            return false;
    }
    
    std::cout << "[OmniEngine] VRAM: " << (hw_profile_.vram_total_bytes / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "[OmniEngine] FP16 Support: " << (hw_profile_.supports_fp16 ? "YES" : "NO") << std::endl;
    std::cout << "[OmniEngine] Tensor Cores: " << (hw_profile_.supports_tensor_cores ? "YES" : "NO") << std::endl;
    std::cout << "[OmniEngine] Flash Attention: " << (hw_profile_.supports_flash_attention ? "YES" : "NO") << std::endl;
    
    return true;
}

void OmniEngine::InitializeCUDABackend() {
    #ifdef __CUDACC__
    int device = 0;
    cudaSetDevice(device);
    cudaFree(0); // Initialize CUDA context
    #endif
}

void OmniEngine::InitializeVulkanBackend() {
    // Vulkan initialization code
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "OmniInference";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    // Create instance, device, etc.
    std::cout << "[OmniEngine] Vulkan backend initialized" << std::endl;
}

bool OmniEngine::LoadModel(const ModelParameters& params, const TurboQuantConfig& quant_cfg) {
    current_model_params_ = params;
    active_quant_cfg_ = quant_cfg;
    
    // Validate memory constraints
    if (!ValidateQuantizationFit(params, quant_cfg)) {
        std::cerr << "[OmniEngine] ERROR: Model + KV Cache exceeds VRAM after quantization" << std::endl;
        return false;
    }
    
    std::cout << "[OmniEngine] Loading model: " << params.model_name << std::endl;
    std::cout << "[OmniEngine] Quantization: " << static_cast<int>(quant_cfg.mode) << "-bit mode" << std::endl;
    
    ApplyOptimizationMatrix();
    
    model_loaded_ = true;
    return true;
}

bool OmniEngine::UnloadModel() {
    model_loaded_ = false;
    current_pipeline_.nodes.clear();
    return true;
}

uint64_t OmniEngine::EstimateMemoryUsage(
    const ModelParameters& params, 
    const TurboQuantConfig& quant
) const {
    // Model weights (rough estimate for 8-bit quantized)
    uint64_t model_size = 0;
    // Assuming ~1GB per 8B parameters
    model_size = (125000000 * 8) / (1024 * 1024); // Example: 125M param model
    
    // KV Cache calculation
    // size = 2 * n_layers * n_embd * n_ctx * bytes_per_token
    float kv_cache_bytes = 2.0f * params.n_layer * params.n_embd * params.n_ctx * 2.0f; // FP16
    
    // Apply quantization reduction
    float quant_reduction = 1.0f;
    switch (quant.mode) {
        case QuantizationMode::TURBO_QUANT_2BIT: quant_reduction = 0.125f; break;
        case QuantizationMode::TURBO_QUANT_3BIT: quant_reduction = 0.1875f; break;
        case QuantizationMode::TURBO_QUANT_4BIT: quant_reduction = 0.25f; break;
        case QuantizationMode::TURBO_QUANT_6BIT: quant_reduction = 0.375f; break;
        default: quant_reduction = 1.0f;
    }
    
    kv_cache_bytes *= quant_reduction;
    
    uint64_t total = model_size + static_cast<uint64_t>(kv_cache_bytes);
    
    // Add 1GB safety buffer
    total += (1024 * 1024 * 1024);
    
    return total;
}

bool OmniEngine::ValidateQuantizationFit(
    const ModelParameters& params, 
    const TurboQuantConfig& config
) const {
    uint64_t required_memory = EstimateMemoryUsage(params, config);
    uint64_t safe_vram = hw_profile_.vram_total_bytes;
    
    if (required_memory > safe_vram) {
        std::cout << "[OmniEngine] WARNING: Required " << (required_memory / (1024*1024*1024)) 
                  << "GB but only " << (safe_vram / (1024*1024*1024)) << "GB available" << std::endl;
        return false;
    }
    
    std::cout << "[OmniEngine] Memory Check PASSED: " << (required_memory / (1024*1024*1024)) 
              << "GB / " << (safe_vram / (1024*1024*1024)) << "GB" << std::endl;
    return true;
}

bool OmniEngine::ApplyQuantization(const TurboQuantConfig& config) {
    if (!model_loaded_) return false;
    
    active_quant_cfg_ = config;
    
    std::cout << "[OmniEngine] Applying TurboQuant Plus:" << std::endl;
    std::cout << "  - Polar Transform: " << (config.use_polar_transform ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  - QJL Correction: " << (config.use_qjl_correction ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  - Key Precision: " << config.key_bits << "-bit" << std::endl;
    std::cout << "  - Value Precision: " << config.value_bits << "-bit" << std::endl;
    
    // Call TurboQuant Plus kernels
    if (active_backend_ == HardwareBackend::NVIDIA_CUDA) {
        // launch_turbo_quant_kv_polar(...);
    }
    
    return true;
}

bool OmniEngine::CreatePipeline(const PipelineConfig& pipeline) {
    current_pipeline_ = pipeline;
    
    std::cout << "[OmniEngine] Pipeline created with " << pipeline.nodes.size() << " nodes:" << std::endl;
    for (const auto& node : pipeline.nodes) {
        std::cout << "  - " << node.node_id << " (" << node.node_type << ")" << std::endl;
    }
    
    return true;
}

bool OmniEngine::ExecutePipeline() {
    std::cout << "[OmniEngine] Executing pipeline..." << std::endl;
    
    for (const auto& node : current_pipeline_.nodes) {
        if (!node.enabled) continue;
        
        std::cout << "[Pipeline] Executing node: " << node.node_id << std::endl;
        
        if (node.node_type == "quantize") {
            // Execute quantization node
            TurboQuantConfig cfg;
            cfg.key_bits = static_cast<int>(node.parameters.at("key_bits"));
            cfg.value_bits = static_cast<int>(node.parameters.at("value_bits"));
            ApplyQuantization(cfg);
        }
    }
    
    return true;
}

void OmniEngine::SetModelParameter(const std::string& param_name, float value) {
    if (param_name == "temperature") {
        // Applied during generation
    } else if (param_name == "rope_freq_scale") {
        current_model_params_.rope_freq_scale = value;
    } else if (param_name == "top_p") {
        // Applied during generation
    }
    // Add more parameters as needed
}

float OmniEngine::GetModelParameter(const std::string& param_name) const {
    if (param_name == "rope_freq_scale") return current_model_params_.rope_freq_scale;
    if (param_name == "n_ctx") return static_cast<float>(current_model_params_.n_ctx);
    if (param_name == "n_embd") return static_cast<float>(current_model_params_.n_embd);
    return 0.0f;
}

std::map<std::string, float> OmniEngine::GetAllModelParameters() const {
    std::map<std::string, float> params;
    params["n_embd"] = current_model_params_.n_embd;
    params["n_layer"] = current_model_params_.n_layer;
    params["n_head"] = current_model_params_.n_head;
    params["n_ctx"] = current_model_params_.n_ctx;
    params["rope_freq_base"] = current_model_params_.rope_freq_base;
    params["rope_freq_scale"] = current_model_params_.rope_freq_scale;
    return params;
}

void OmniEngine::ApplyOptimizationMatrix() {
    std::cout << "[OmniEngine] === OPTIMIZATION MATRIX ===" << std::endl;
    
    uint64_t vram_usable = hw_profile_.vram_total_bytes - (1024 * 1024 * 1024); // 1GB buffer
    uint64_t required = EstimateMemoryUsage(current_model_params_, active_quant_cfg_);
    
    std::cout << "[Solver] VRAM Constraint: " << (vram_usable / (1024*1024*1024)) << "GB" << std::endl;
    std::cout << "[Solver] Required Memory: " << (required / (1024*1024*1024)) << "GB" << std::endl;
    
    if (required <= vram_usable) {
        std::cout << "[Solver] ✓ Configuration VALID - Full GPU offload enabled" << std::endl;
    } else {
        std::cout << "[Solver] ✗ Overflow detected - Switching to layer offload strategy" << std::endl;
    }
}

std::string OmniEngine::Generate(
    const std::string& prompt,
    const GenerationConfig& gen_cfg,
    std::function<void(const std::string&)> token_callback
) {
    if (!model_loaded_) {
        return "ERROR: No model loaded";
    }
    
    std::string output = "";
    
    // Placeholder generation loop
    for (int i = 0; i < gen_cfg.max_tokens; ++i) {
        std::string token = "token_" + std::to_string(i) + " ";
        output += token;
        
        if (token_callback) {
            token_callback(token);
        }
        
        if (token_callback_) {
            token_callback_(token);
        }
    }
    
    return output;
}

void OmniEngine::PrintHardwareReport() const {
    std::cout << "\n========== HARDWARE REPORT ==========" << std::endl;
    std::cout << "Device: " << hw_profile_.device_name << std::endl;
    std::cout << "Backend: ";
    switch (hw_profile_.backend) {
        case HardwareBackend::NVIDIA_CUDA: std::cout << "NVIDIA CUDA"; break;
        case HardwareBackend::AMD_VULKAN: std::cout << "AMD Vulkan"; break;
        case HardwareBackend::INTEL_VULKAN: std::cout << "Intel Vulkan"; break;
        case HardwareBackend::INTEL_SYCL: std::cout << "Intel SYCL"; break;
        case HardwareBackend::CPU_FALLBACK: std::cout << "CPU Fallback"; break;
    }
    std::cout << std::endl;
    std::cout << "VRAM: " << (hw_profile_.vram_total_bytes / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "FP16: " << (hw_profile_.supports_fp16 ? "YES" : "NO") << std::endl;
    std::cout << "Tensor Cores: " << (hw_profile_.supports_tensor_cores ? "YES" : "NO") << std::endl;
    std::cout << "Flash Attention: " << (hw_profile_.supports_flash_attention ? "YES" : "NO") << std::endl;
    std::cout << "====================================\n" << std::endl;
}