#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <json.hpp>

using json = nlohmann::json;

// ============================================================
// PERFORMANCE MONITORING & METRICS
// ============================================================

struct PerformanceMetrics {
    double tokens_per_second = 0.0;
    double latency_ms = 0.0;
    double gpu_utilization = 0.0;
    double gpu_memory_used_mb = 0.0;
    double cpu_utilization = 0.0;
    double system_memory_used_mb = 0.0;
    int requests_processed = 0;
    int errors_count = 0;
};

struct RequestMetrics {
    std::string request_id;
    std::string endpoint;
    std::chrono::system_clock::time_point timestamp;
    double latency_ms = 0.0;
    int tokens_processed = 0;
    bool success = true;
    std::string error_message;
};

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor() = default;

    void RecordRequest(const RequestMetrics& metrics);
    PerformanceMetrics GetMetrics() const;
    json ExportMetricsJSON() const;

    void StartMeasurement();
    void EndMeasurement(const std::string& label);

    double GetAverageTPS() const;
    double GetAverageLatency() const;
    int GetTotalRequests() const;

private:
    std::vector<RequestMetrics> request_history_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> measurements_;

    double CalculateAverageTPS() const;
    double CalculateAverageLatency() const;
};