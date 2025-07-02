#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <map>
#include <vector>
#include <algorithm>
#include <mutex>
#include <unordered_map>

// 新增：内存存储结构
struct CpuMetric {
    long long timestamp;
    double usage_percent;
    double load_avg_1m;
    double load_avg_5m;
    double load_avg_15m;
    int core_count;
};
struct MemoryMetric {
    long long timestamp;
    uint64_t total;
    uint64_t used;
    uint64_t free;
    double usage_percent;
};
std::unordered_map<std::string, CpuMetric> latest_cpu_metrics_ = {};
std::unordered_map<std::string, MemoryMetric> latest_memory_metrics_ = {};
std::mutex cpu_metric_mutex_;
std::mutex memory_metric_mutex_;

bool DatabaseManager::initializeMetricTables()
{
    return true;
}

bool DatabaseManager::saveCpuMetrics(const std::string &node_id,
                                     long long timestamp,
                                     const nlohmann::json &cpu_data)
{
    std::lock_guard<std::mutex> lock(cpu_metric_mutex_);
    if (!cpu_data.contains("usage_percent") || !cpu_data.contains("load_avg_1m") ||
        !cpu_data.contains("load_avg_5m") || !cpu_data.contains("load_avg_15m") ||
        !cpu_data.contains("core_count"))
    {
        return false;
    }
    CpuMetric metric = {
        .timestamp = 0,
        .usage_percent = 0.0,
        .load_avg_1m = 0.0,
        .load_avg_5m = 0.0,
        .load_avg_15m = 0.0,
        .core_count = 0
    };
    metric.timestamp = timestamp;
    metric.usage_percent = cpu_data["usage_percent"].get<double>();
    metric.load_avg_1m = cpu_data["load_avg_1m"].get<double>();
    metric.load_avg_5m = cpu_data["load_avg_5m"].get<double>();
    metric.load_avg_15m = cpu_data["load_avg_15m"].get<double>();
    metric.core_count = cpu_data["core_count"].get<int>();
    latest_cpu_metrics_[node_id] = metric;
    return true;
}

bool DatabaseManager::saveMemoryMetrics(const std::string &node_id,
                                        long long timestamp,
                                        const nlohmann::json &memory_data)
{
    std::lock_guard<std::mutex> lock(memory_metric_mutex_);
    if (!memory_data.contains("total") || !memory_data.contains("used") ||
        !memory_data.contains("free") || !memory_data.contains("usage_percent"))
    {
        return false;
    }
    MemoryMetric metric = {
        .timestamp = 0,
        .total = 0,
        .used = 0,
        .free = 0,
        .usage_percent = 0.0
    };
    metric.timestamp = timestamp;
    metric.total = memory_data["total"].get<uint64_t>();
    metric.used = memory_data["used"].get<uint64_t>();
    metric.free = memory_data["free"].get<uint64_t>();
    metric.usage_percent = memory_data["usage_percent"].get<double>();
    latest_memory_metrics_[node_id] = metric;
    return true;
}

nlohmann::json DatabaseManager::getCpuMetrics(const std::string &node_id)
{
    std::lock_guard<std::mutex> lock(cpu_metric_mutex_);
    nlohmann::json result = nlohmann::json::array();
    auto it = latest_cpu_metrics_.find(node_id);
    if (it != latest_cpu_metrics_.end()) {
        nlohmann::json metric = nlohmann::json::object();
        metric["timestamp"] = it->second.timestamp;
        metric["usage_percent"] = it->second.usage_percent;
        metric["load_avg_1m"] = it->second.load_avg_1m;
        metric["load_avg_5m"] = it->second.load_avg_5m;
        metric["load_avg_15m"] = it->second.load_avg_15m;
        metric["core_count"] = it->second.core_count;
        result.push_back(metric);
    }
    return result;
}

nlohmann::json DatabaseManager::getMemoryMetrics(const std::string &node_id)
{
    std::lock_guard<std::mutex> lock(memory_metric_mutex_);
    nlohmann::json result = nlohmann::json::array();
    auto it = latest_memory_metrics_.find(node_id);
    if (it != latest_memory_metrics_.end()) {
        nlohmann::json metric = nlohmann::json::object();
        metric["timestamp"] = it->second.timestamp;
        metric["total"] = it->second.total;
        metric["used"] = it->second.used;
        metric["free"] = it->second.free;
        metric["usage_percent"] = it->second.usage_percent;
        result.push_back(metric);
    }
    return result;
}

bool DatabaseManager::saveResourceUsage(const nlohmann::json &resource_usage)
{
    // 检查必要字段
    if (!resource_usage.contains("node_id") || !resource_usage.contains("timestamp") || !resource_usage.contains("resource")) {
        return false;
    }
    std::string node_id = resource_usage["node_id"];
    long long timestamp = resource_usage["timestamp"];
    const auto& resource = resource_usage["resource"];

    // 保存各类资源数据
    if (resource.contains("cpu")) {
        saveCpuMetrics(node_id, timestamp, resource["cpu"]);
    }
    if (resource.contains("memory")) {
        saveMemoryMetrics(node_id, timestamp, resource["memory"]);
    }
    
    return true;
}