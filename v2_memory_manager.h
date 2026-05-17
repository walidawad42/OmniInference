#pragma once

#include <string>
#include <vector>
#include <map>
#include <json.hpp>
#include <cstdint>
#include <functional>

using json = nlohmann::json;

// ============================================================
// MEMORY MANAGEMENT SYSTEM
// Objective: Maximize TPS + Error Elimination
// Deterministic Buffer: 200MB to 1GB (USER CONTROLLED)
// ============================================================

enum class MemoryTier {
    GPU_VRAM,
    SYSTEM_RAM,
    DISK_SWAP
};

enum class BufferWarningLevel {
    SAFE,           // Buffer > 50% of total VRAM
    CAUTION,        // Buffer 30-50% of total VRAM
    WARNING,        // Buffer 10-30% of total VRAM
    CRITICAL        // Buffer < 10% of total VRAM
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
    std::string purpose; // "model", "kv_cache", "buffer", "workspace"
    bool is_reserved = false;
    bool is_pinned = false;
};

struct MemoryPolicy {
    // Buffer configuration (200MB to 1GB)
    uint64_t vram_buffer_size_min = 200 * 1024 * 1024;    // 200MB minimum
    uint64_t vram_buffer_size_max = 1024 * 1024 * 1024;   // 1GB maximum
    uint64_t vram_buffer_size = 1024 * 1024 * 1024;       // 1GB default
    
    // TPS Optimization
    bool optimize_for_throughput = true;
    bool use_pinned_memory = true;
    bool enable_memory_pooling = true;
    bool enable_layer_reuse = true;
    
    // Overflow configuration
    uint64_t ram_max_allocation = 0; // 0 = unlimited
    bool enable_disk_swap = false;
    std::string swap_directory = "./swap";
    float utilization_threshold = 0.85f;
    
    // Error elimination
    bool validate_allocations = true;
    bool check_fragmentation = true;
    bool track_allocations = true;
};

struct TPSMetrics {
    double current_tps = 0.0;
    double average_tps = 0.0;
    double peak_tps = 0.0;
    double memory_overhead_percent = 0.0;
    double allocation_time_us = 0.0;
    int allocation_count = 0;
    int reallocation_count = 0;
    bool is_optimal = false;
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    // ========== INITIALIZATION ==========
    bool Initialize(const MemoryPolicy& policy);
    void SetPolicy(const MemoryPolicy& policy);

    // ========== BUFFER CONFIGURATION ==========
    bool SetDeterministicBuffer(uint64_t size_bytes);
    uint64_t GetDeterministicBuffer() const { return policy_.vram_buffer_size; }
    BufferWarningLevel GetBufferWarningLevel() const;
    std::string GetBufferWarning() const;
    bool IsBufferSafe() const { return GetBufferWarningLevel() == BufferWarningLevel::SAFE; }

    // ========== VRAM MANAGEMENT ==========
    uint64_t GetTotalVRAM() const { return total_vram_bytes_; }
    uint64_t GetAvailableVRAM() const { return available_vram_bytes_; }
    uint64_t GetReservedBuffer() const { return policy_.vram_buffer_size; }
    uint64_t GetUsableVRAM() const;
    float GetVRAMUtilizationPercent() const;

    bool AllocateVRAM(const std::string& allocation_id, uint64_t size, const std::string& purpose);
    bool DeallocateVRAM(const std::string& allocation_id);
    bool CanAllocateVRAM(uint64_t size) const;

    // ========== OVERFLOW HANDLING ==========
    bool CheckAndHandleOverflow(uint64_t required_bytes);
    MemoryTier GetBestTierForAllocation(uint64_t size);
    bool OffloadToRAM(const std::string& allocation_id);
    bool OffloadToDisk(const std::string& allocation_id);
    bool RebalanceMemory();

    // ========== SYSTEM RAM MANAGEMENT ==========
    uint64_t GetTotalSystemRAM() const { return total_system_ram_bytes_; }
    uint64_t GetAvailableSystemRAM() const { return available_system_ram_bytes_; }
    bool AllocateSystemRAM(const std::string& allocation_id, uint64_t size);

    // ========== TPS OPTIMIZATION ==========
    TPSMetrics GetTPSMetrics() const;
    void RecordAllocationTime(double time_us);
    void RecordTPSMeasurement(double tps);
    bool OptimizeForThroughput();
    double EstimatePeakTPS(uint64_t model_size) const;

    // ========== ERROR ELIMINATION ==========
    bool ValidateAllocations();
    bool CheckMemoryFragmentation();
    json GetMemoryHealthReport() const;
    bool RecoverFromError();

    // ========== STATISTICS ==========
    MemoryBuffer GetBufferStats() const;
    std::vector<VRAMAllocation> GetAllocations() const;
    json ExportMemoryStatistics() const;

    // ========== PREDICTION ==========
    bool CanFitModel(uint64_t model_size, int context_length) const;
    uint64_t EstimateKVCacheSize(int n_layers, int n_embd, int context_length) const;
    uint64_t EstimateTotalMemory(uint64_t model_size, int context_length) const;

    // ========== CALLBACKS ==========
    using OverflowCallback = std::function<void(uint64_t, const std::string&)>;
    using WarningCallback = std::function<void(const std::string&, BufferWarningLevel)>;
    
    void SetOverflowCallback(OverflowCallback callback) { overflow_callback_ = callback; }
    void SetWarningCallback(WarningCallback callback) { warning_callback_ = callback; }

private:
    // Hardware stats
    uint64_t total_vram_bytes_ = 0;
    uint64_t available_vram_bytes_ = 0;
    uint64_t total_system_ram_bytes_ = 0;
    uint64_t available_system_ram_bytes_ = 0;

    // Memory management
    uint64_t allocated_vram_bytes_ = 0;
    uint64_t allocated_ram_bytes_ = 0;
    uint64_t disk_swap_used_bytes_ = 0;

    // Allocations tracking
    std::map<std::string, VRAMAllocation> vram_allocations_;
    std::map<std::string, VRAMAllocation> ram_allocations_;

    // TPS metrics
    TPSMetrics tps_metrics_;
    std::vector<double> tps_history_;
    std::vector<double> allocation_time_history_;

    // Policy
    MemoryPolicy policy_;
    OverflowCallback overflow_callback_;
    WarningCallback warning_callback_;

    // Helper methods
    void QueryHardwareMemory();
    void UpdateAvailableMemory();
    void MonitorFragmentation();
    bool CreateSwapDirectory();
    void UpdateTPSMetrics();
};