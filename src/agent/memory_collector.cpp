#include "memory_collector.h"
#include <fstream>
#include <iostream>
#include <sstream>

nlohmann::json MemoryCollector::collect() {
    nlohmann::json result = nlohmann::json::object();
    
    // 获取内存信息
    unsigned long long total = 0, free = 0, available = 0;
    if (getMemoryInfo(total, free, available)) {
        // 计算已使用内存
        unsigned long long used = total - available;
        
        // 计算使用率，防止除零错误
        double usage_percent = 0.0;
        if (total > 0) {
            usage_percent = 100.0 * static_cast<double>(used) / static_cast<double>(total);
        }
        
        // 构建JSON结果
        result["total"] = total * 1024;  // 转换为字节
        result["used"] = used * 1024;    // 转换为字节
        result["free"] = free * 1024;    // 转换为字节
        result["usage_percent"] = usage_percent;
    }
    
    return result;
}

std::string MemoryCollector::getType() const {
    return "memory";
}

bool MemoryCollector::getMemoryInfo(unsigned long long& total, unsigned long long& free, unsigned long long& available) {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return false;
    }
    
    std::string line = "";
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key = "";
        unsigned long long value = 0;
        std::string unit = "";
        
        iss >> key >> value >> unit;
        
        if (key == "MemTotal:") {
            total = value;
        } else if (key == "MemFree:") {
            free = value;
        } else if (key == "MemAvailable:") {
            available = value;
        }
    }
    
    // 如果没有MemAvailable字段（较旧的内核），使用MemFree作为近似值
    if (available == 0) {
        available = free;
    }
    
    return (total > 0);
}
