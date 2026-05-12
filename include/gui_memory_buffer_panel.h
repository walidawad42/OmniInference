#pragma once

#include "memory_manager.h"
#include "gui_imgui_compat.h"
#include <vector>
#include <string>

class GUIMemoryBufferPanel {
public:
    GUIMemoryBufferPanel();
    ~GUIMemoryBufferPanel() = default;

    void Render();
    void SetMemoryManager(MemoryManager* mm) { memory_manager_ = mm; }

private:
    MemoryManager* memory_manager_ = nullptr;

    // UI State
    bool show_allocation_details_ = false;
    bool enable_deterministic_buffer_ = true;
    uint64_t buffer_size_gb_ = 1;
    float overflow_threshold_ = 0.85f;
    bool enable_ram_overflow_ = true;
    bool enable_disk_swap_ = true;

    // Visualization
    std::vector<float> vram_history_;
    std::vector<float> ram_history_;
    std::vector<float> utilization_history_;

    // Rendering methods
    void RenderBufferConfiguration();
    void RenderVRAMStatus();
    void RenderSystemRAMStatus();
    void RenderDiskSwapStatus();
    void RenderAllocationTable();
    void RenderMemoryFlowVisualization();
    void RenderOverflowSimulation();
    void RenderMemoryPrediction();
    void RenderMemoryTiering();

    // Helper functions
    void UpdateHistories();
    ImVec4 GetTierColor(MemoryTier tier);
    std::string FormatBytes(uint64_t bytes);
};