#include "memory_manager.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/stat.h>   // mkdir
    #include <sys/sysinfo.h>
    #include <unistd.h>
#endif

MemoryManager::MemoryManager() {
    QueryHardwareMemory();
}

MemoryManager::~MemoryManager() = default;

void MemoryManager::QueryHardwareMemory() {
    #ifdef _WIN32
        MEMORYSTATUSEX stat;
        stat.dwLength = sizeof(stat);
        GlobalMemoryStatusEx(&stat);
        total_system_ram_bytes_ = stat.ullTotalPhys;
        available_system_ram_bytes_ = stat.ullAvailPhys;
    #else
        struct sysinfo info;
        sysinfo(&info);
        total_system_ram_bytes_ = info.totalram * info.mem_unit;
        available_system_ram_bytes_ = info.freeram * info.mem_unit;
    #endif

    #ifdef __CUDACC__
        size_t free_bytes, total_bytes;
        cudaMemGetInfo(&free_bytes, &total_bytes);
        total_vram_bytes_ = total_bytes;
        available_vram_bytes_ = free_bytes;
    #endif

    std::cout << "[MemoryManager] System RAM: " << (total_system_ram_bytes_ / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "[MemoryManager] VRAM: " << (total_vram_bytes_ / (1024*1024*1024)) << " GB" << std::endl;
}

bool MemoryManager::Initialize(const MemoryPolicy& policy) {
    policy_ = policy;
    reserved_buffer_bytes_ = policy_.vram_buffer_size;

    std::cout << "[MemoryManager] Initialized with policy:" << std::endl;
    std::cout << "[MemoryManager]   - Reserved Buffer: " << (reserved_buffer_bytes_ / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "[MemoryManager]   - Usable VRAM: " << (GetUsableVRAM() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "[MemoryManager]   - Enable Disk Swap: " << (policy_.enable_disk_swap ? "YES" : "NO") << std::endl;

    if (policy_.enable_disk_swap) {
        CreateSwapDirectory();
    }

    return true;
}

void MemoryManager::SetPolicy(const MemoryPolicy& policy) {
    policy_ = policy;
    reserved_buffer_bytes_ = policy_.vram_buffer_size;
}

uint64_t MemoryManager::GetUsableVRAM() const {
    // `total_vram_bytes_` and the values being subtracted are unsigned, so the
    // previous `total_vram_bytes_ - reserved_buffer_bytes_ - allocated_vram_bytes_`
    // would silently underflow when more VRAM was reserved + allocated than is
    // physically present (e.g. CPU-only builds where total VRAM is 0). Guard
    // explicitly and return 0 in that case.
    const uint64_t reserved = reserved_buffer_bytes_ + allocated_vram_bytes_;
    if (total_vram_bytes_ <= reserved) {
        return 0;
    }
    return total_vram_bytes_ - reserved;
}

bool MemoryManager::AllocateVRAM(const std::string& allocation_id, uint64_t size, const std::string& purpose) {
    if (!CanAllocateVRAM(size)) {
        std::cerr << "[MemoryManager] Cannot allocate " << (size / (1024*1024)) << "MB - insufficient VRAM" << std::endl;

        // Try to overflow to RAM
        if (CheckAndHandleOverflow(size)) {
            std::cout << "[MemoryManager] ✓ Allocated to system RAM after overflow" << std::endl;
            return true;
        }

        return false;
    }

    VRAMAllocation allocation;
    allocation.allocation_id = allocation_id;
    allocation.size_bytes = size;
    allocation.offset = allocated_vram_bytes_;
    allocation.purpose = purpose;

    vram_allocations_[allocation_id] = allocation;
    allocated_vram_bytes_ += size;

    std::cout << "[MemoryManager] ✓ Allocated " << (size / (1024*1024)) << "MB VRAM for " << purpose << std::endl;

    return true;
}

bool MemoryManager::DeallocateVRAM(const std::string& allocation_id) {
    auto it = vram_allocations_.find(allocation_id);
    if (it == vram_allocations_.end()) {
        return false;
    }

    allocated_vram_bytes_ -= it->second.size_bytes;
    vram_allocations_.erase(it);

    std::cout << "[MemoryManager] Deallocated VRAM: " << allocation_id << std::endl;

    return true;
}

bool MemoryManager::CanAllocateVRAM(uint64_t size) const {
    return size <= GetUsableVRAM();
}

bool MemoryManager::CheckAndHandleOverflow(uint64_t required_bytes) {
    float current_utilization = static_cast<float>(allocated_vram_bytes_) / total_vram_bytes_;

    std::cout << "[MemoryManager] Current utilization: " << (current_utilization * 100) << "%" << std::endl;

    if (current_utilization >= policy_.utilization_threshold) {
        std::cout << "[MemoryManager] ⚠️ VRAM utilization threshold exceeded!" << std::endl;
        std::cout << "[MemoryManager] Attempting overflow to system RAM..." << std::endl;

        if (overflow_callback_) {
            overflow_callback_(required_bytes, "VRAM threshold exceeded");
        }

        return OffloadToRAM("");
    }

    return false;
}

MemoryTier MemoryManager::GetBestTierForAllocation(uint64_t size) {
    // Priority: VRAM > System RAM > Disk

    if (CanAllocateVRAM(size)) {
        return MemoryTier::GPU_VRAM;
    }

    if (available_system_ram_bytes_ >= size) {
        return MemoryTier::SYSTEM_RAM;
    }

    if (policy_.enable_disk_swap) {
        return MemoryTier::DISK_SWAP;
    }

    return MemoryTier::GPU_VRAM; // Fallback
}

bool MemoryManager::OffloadToRAM(const std::string& allocation_id) {
    std::cout << "[MemoryManager] Offloading to system RAM..." << std::endl;

    if (allocation_id.empty()) {
        // Find largest VRAM allocation and offload it
        uint64_t largest_size = 0;
        std::string largest_id;

        for (const auto& [id, alloc] : vram_allocations_) {
            if (alloc.size_bytes > largest_size && alloc.purpose != "system") {
                largest_size = alloc.size_bytes;
                largest_id = id;
            }
        }

        if (!largest_id.empty()) {
            return OffloadToRAM(largest_id);
        }

        return false;
    }

    auto it = vram_allocations_.find(allocation_id);
    if (it == vram_allocations_.end()) {
        return false;
    }

    uint64_t size = it->second.size_bytes;
    std::string purpose = it->second.purpose;

    // Move to RAM
    vram_allocations_.erase(it);
    allocated_vram_bytes_ -= size;

    VRAMAllocation ram_alloc;
    ram_alloc.allocation_id = allocation_id;
    ram_alloc.size_bytes = size;
    ram_alloc.purpose = purpose;

    ram_allocations_[allocation_id] = ram_alloc;
    allocated_ram_bytes_ += size;
    available_system_ram_bytes_ -= size;

    std::cout << "[MemoryManager] ✓ Offloaded " << (size / (1024*1024)) << "MB to system RAM" << std::endl;

    return true;
}

bool MemoryManager::OffloadToDisk(const std::string& allocation_id) {
    std::cout << "[MemoryManager] Offloading to disk swap..." << std::endl;

    auto it = vram_allocations_.find(allocation_id);
    if (it == vram_allocations_.end()) {
        return false;
    }

    uint64_t size = it->second.size_bytes;

    vram_allocations_.erase(it);
    allocated_vram_bytes_ -= size;
    disk_swap_used_bytes_ += size;

    std::cout << "[MemoryManager] ✓ Offloaded " << (size / (1024*1024)) << "MB to disk" << std::endl;

    return true;
}

bool MemoryManager::AllocateSystemRAM(const std::string& allocation_id, uint64_t size) {
    if (size > available_system_ram_bytes_) {
        return false;
    }

    VRAMAllocation allocation;
    allocation.allocation_id = allocation_id;
    allocation.size_bytes = size;
    allocation.purpose = "system_ram";

    ram_allocations_[allocation_id] = allocation;
    allocated_ram_bytes_ += size;
    available_system_ram_bytes_ -= size;

    return true;
}

bool MemoryManager::EnableDiskSwap(const std::string& swap_path) {
    policy_.swap_directory = swap_path;
    policy_.enable_disk_swap = true;
    return CreateSwapDirectory();
}

bool MemoryManager::DisableDiskSwap() {
    policy_.enable_disk_swap = false;
    return true;
}

bool MemoryManager::CanFitModel(uint64_t model_size, int context_length) const {
    uint64_t kv_cache_size = EstimateKVCacheSize(32, 4096, context_length);
    uint64_t total_required = model_size + kv_cache_size;

    return total_required <= (total_vram_bytes_ - reserved_buffer_bytes_);
}

uint64_t MemoryManager::EstimateKVCacheSize(int n_layers, int n_embd, int context_length) const {
    // KV cache = 2 * n_layers * n_embd * context_length * sizeof(float16)
    // sizeof(float16) = 2 bytes
    return static_cast<uint64_t>(2 * n_layers * n_embd * context_length * 2);
}

uint64_t MemoryManager::EstimateTotalMemory(uint64_t model_size, int context_length) const {
    uint64_t kv_size = EstimateKVCacheSize(32, 4096, context_length);
    return model_size + kv_size + reserved_buffer_bytes_;
}

MemoryBuffer MemoryManager::GetBufferStats() const {
    MemoryBuffer stats;
    stats.size_bytes = total_vram_bytes_;
    stats.allocated_bytes = allocated_vram_bytes_;
    stats.utilization_percent = (static_cast<float>(allocated_vram_bytes_) / total_vram_bytes_) * 100.0f;
    stats.is_locked = true;

    return stats;
}

std::vector<VRAMAllocation> MemoryManager::GetAllocations() const {
    std::vector<VRAMAllocation> allocations;
    for (const auto& [id, alloc] : vram_allocations_) {
        allocations.push_back(alloc);
    }
    return allocations;
}

json MemoryManager::ExportMemoryStatistics() const {
    json stats;
    stats["total_vram_gb"] = total_vram_bytes_ / (1024.0*1024.0*1024.0);
    stats["allocated_vram_gb"] = allocated_vram_bytes_ / (1024.0*1024.0*1024.0);
    stats["usable_vram_gb"] = GetUsableVRAM() / (1024.0*1024.0*1024.0);
    stats["reserved_buffer_gb"] = reserved_buffer_bytes_ / (1024.0*1024.0*1024.0);
    stats["utilization_percent"] = (static_cast<float>(allocated_vram_bytes_) / total_vram_bytes_) * 100.0f;

    stats["system_ram_total_gb"] = total_system_ram_bytes_ / (1024.0*1024.0*1024.0);
    stats["system_ram_available_gb"] = available_system_ram_bytes_ / (1024.0*1024.0*1024.0);
    stats["system_ram_allocated_gb"] = allocated_ram_bytes_ / (1024.0*1024.0*1024.0);

    stats["disk_swap_enabled"] = policy_.enable_disk_swap;
    stats["disk_swap_used_gb"] = disk_swap_used_bytes_ / (1024.0*1024.0*1024.0);

    stats["allocations"] = json::array();
    for (const auto& [id, alloc] : vram_allocations_) {
        json alloc_json;
        alloc_json["id"] = id;
        alloc_json["size_mb"] = alloc.size_bytes / (1024.0*1024.0);
        alloc_json["purpose"] = alloc.purpose;
        stats["allocations"].push_back(alloc_json);
    }

    return stats;
}

bool MemoryManager::CreateSwapDirectory() {
    #ifdef _WIN32
        return CreateDirectoryA(policy_.swap_directory.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
    #else
        return mkdir(policy_.swap_directory.c_str(), 0755) == 0 || errno == EEXIST;
    #endif
}

void MemoryManager::UpdateAvailableMemory() {
    QueryHardwareMemory();
}