#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

// ============================================================
// MEMORY MANAGEMENT SYSTEM
// Deterministic Buffer Management + RAM Overflow
// ============================================================

enum class MemoryTier {
    GPU_VRAM,
    SYSTEM_RAM,
    DISK_SWAP
};

struct MemoryBuffer {
    std::string buffer_id;
    MemoryTier tier;
    uint64_t size_bytes = 0;
    uint64_t allocated_bytes = 0;
    bool is_pinned = false;
    bool is_locked = true;
    float utilization_percent = 0.0f;
};

struct VRAMAllocation {
    std::string allocation_id;
    uint64_t size_bytes = 0;
    uint64_t offset = 0;
    std::string purpose; // "model", "kv_cache", "buffer", "system"
    bool is_reserved = false;
};

struct MemoryPolicy {
    uint64_t vram_buffer_size = 1024 * 1024 * 1024; // 1GB default
    uint64_t ram_max_allocation = 0; // 0 = unlimited
    bool enable_disk_swap = true;
    std::string swap_directory = "./swap";
    float utilization_threshold = 0.85f; // Start overflow at 85%
    bool force_pinned_memory = false;
    // When true, MemoryManager prefers throughput-friendly allocations
    // (larger contiguous chunks, pinned memory) over space efficiency.
    bool optimize_for_throughput = false;
};

// Lightweight tokens-per-second telemetry surfaced to the GUI.
struct TPSMetrics {
    float current_tps = 0.0f;
    float average_tps = 0.0f;
    float memory_overhead_percent = 0.0f;
    bool is_optimal = false;
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    // ========== INITIALIZATION ==========
    bool Initialize(const MemoryPolicy& policy);
    void SetPolicy(const MemoryPolicy& policy);

    // ========== VRAM MANAGEMENT ==========
    uint64_t GetTotalVRAM() const { return total_vram_bytes_; }
    uint64_t GetAvailableVRAM() const { return available_vram_bytes_; }
    uint64_t GetReservedBuffer() const { return reserved_buffer_bytes_; }
    uint64_t GetUsableVRAM() const;

    bool AllocateVRAM(const std::string& allocation_id, uint64_t size, const std::string& purpose);
    bool DeallocateVRAM(const std::string& allocation_id);
    bool CanAllocateVRAM(uint64_t size) const;

    // ========== OVERFLOW HANDLING ==========
    bool CheckAndHandleOverflow(uint64_t required_bytes);
    MemoryTier GetBestTierForAllocation(uint64_t size);
    bool OffloadToRAM(const std::string& allocation_id);
    bool OffloadToDisk(const std::string& allocation_id);

    // ========== SYSTEM RAM MANAGEMENT ==========
    uint64_t GetTotalSystemRAM() const { return total_system_ram_bytes_; }
    uint64_t GetAvailableSystemRAM() const { return available_system_ram_bytes_; }
    bool AllocateSystemRAM(const std::string& allocation_id, uint64_t size);

    // ========== DISK SWAP MANAGEMENT ==========
    bool EnableDiskSwap(const std::string& swap_path);
    bool DisableDiskSwap();
    uint64_t GetSwapUsage() const { return disk_swap_used_bytes_; }

    // ========== STATISTICS ==========
    MemoryBuffer GetBufferStats() const;
    std::vector<VRAMAllocation> GetAllocations() const;
    json ExportMemoryStatistics() const;
    json GetMemoryHealthReport() const;
    float GetVRAMUtilizationPercent() const;
    TPSMetrics GetTPSMetrics() const;

    // ========== TUNING ==========
    bool SetDeterministicBuffer(uint64_t reserved_bytes);
    void OptimizeForThroughput();
    void RecordTokenThroughput(float tokens_per_second);

    // ========== PREDICTION ==========
    bool CanFitModel(uint64_t model_size, int context_length) const;
    uint64_t EstimateKVCacheSize(int n_layers, int n_embd, int context_length) const;
    uint64_t EstimateTotalMemory(uint64_t model_size, int context_length) const;

    // ========== CALLBACKS ==========
    using OverflowCallback = std::function<void(uint64_t, const std::string&)>;
    void SetOverflowCallback(OverflowCallback callback) { overflow_callback_ = callback; }

private:
    // Hardware stats
    uint64_t total_vram_bytes_ = 0;
    uint64_t available_vram_bytes_ = 0;
    uint64_t total_system_ram_bytes_ = 0;
    uint64_t available_system_ram_bytes_ = 0;

    // Memory management
    uint64_t reserved_buffer_bytes_ = 0;
    uint64_t allocated_vram_bytes_ = 0;
    uint64_t allocated_ram_bytes_ = 0;
    uint64_t disk_swap_used_bytes_ = 0;

    // Allocations tracking
    std::map<std::string, VRAMAllocation> vram_allocations_;
    std::map<std::string, VRAMAllocation> ram_allocations_;

    // Policy
    MemoryPolicy policy_;
    OverflowCallback overflow_callback_;

    // Throughput telemetry (latest sample + EMA average).
    float last_tps_ = 0.0f;
    float average_tps_ = 0.0f;
    uint64_t tps_sample_count_ = 0;

    // Helper methods
    void QueryHardwareMemory();
    void UpdateAvailableMemory();
    bool CreateSwapDirectory();
};