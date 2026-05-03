#include "monitoring.h"

#include <algorithm>
#include <numeric>

PerformanceMonitor::PerformanceMonitor() = default;

void PerformanceMonitor::RecordRequest(const RequestMetrics& metrics) {
    request_history_.push_back(metrics);
}

PerformanceMetrics PerformanceMonitor::GetMetrics() const {
    PerformanceMetrics metrics;
    metrics.requests_processed = static_cast<int>(request_history_.size());
    metrics.errors_count = static_cast<int>(std::count_if(
        request_history_.begin(),
        request_history_.end(),
        [](const RequestMetrics& m) { return !m.success; }));
    metrics.latency_ms = CalculateAverageLatency();
    metrics.tokens_per_second = CalculateAverageTPS();
    return metrics;
}

json PerformanceMonitor::ExportMetricsJSON() const {
    PerformanceMetrics metrics = GetMetrics();
    json out;
    out["tokens_per_second"] = metrics.tokens_per_second;
    out["latency_ms"] = metrics.latency_ms;
    out["gpu_utilization"] = metrics.gpu_utilization;
    out["gpu_memory_used_mb"] = metrics.gpu_memory_used_mb;
    out["cpu_utilization"] = metrics.cpu_utilization;
    out["system_memory_used_mb"] = metrics.system_memory_used_mb;
    out["requests_processed"] = metrics.requests_processed;
    out["errors_count"] = metrics.errors_count;
    return out;
}

void PerformanceMonitor::StartMeasurement() {
    measurements_["__default"] = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::EndMeasurement(const std::string& label) {
    auto it = measurements_.find("__default");
    if (it == measurements_.end()) {
        return;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - it->second).count();
    measurements_.erase(it);

    RequestMetrics m;
    m.request_id = label;
    m.endpoint = label;
    m.timestamp = std::chrono::system_clock::now();
    m.latency_ms = static_cast<double>(duration_ms);
    m.success = true;
    request_history_.push_back(m);
}

double PerformanceMonitor::GetAverageTPS() const {
    return CalculateAverageTPS();
}

double PerformanceMonitor::GetAverageLatency() const {
    return CalculateAverageLatency();
}

int PerformanceMonitor::GetTotalRequests() const {
    return static_cast<int>(request_history_.size());
}

double PerformanceMonitor::CalculateAverageTPS() const {
    if (request_history_.empty()) return 0.0;
    double total_tokens = 0.0;
    double total_seconds = 0.0;
    for (const auto& r : request_history_) {
        total_tokens += r.tokens_processed;
        total_seconds += r.latency_ms / 1000.0;
    }
    return total_seconds > 0.0 ? total_tokens / total_seconds : 0.0;
}

double PerformanceMonitor::CalculateAverageLatency() const {
    if (request_history_.empty()) return 0.0;
    double sum = std::accumulate(
        request_history_.begin(),
        request_history_.end(),
        0.0,
        [](double acc, const RequestMetrics& m) { return acc + m.latency_ms; });
    return sum / static_cast<double>(request_history_.size());
}
