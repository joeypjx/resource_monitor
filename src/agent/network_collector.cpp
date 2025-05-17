#include "network_collector.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <chrono>

NetworkCollector::NetworkCollector() : last_collect_time_(std::chrono::steady_clock::now()) {
    // 初始化时先采集一次网络统计信息，为计算速率做准备
    std::vector<std::string> interfaces = getNetworkInterfaces();
    for (const auto& interface : interfaces) {
        std::map<std::string, unsigned long long> stats;
        if (getInterfaceStats(interface, stats)) {
            last_stats_[interface] = stats;
        }
    }
}

nlohmann::json NetworkCollector::collect() {
    nlohmann::json result = nlohmann::json::array();
    
    // 获取当前时间
    auto current_time = std::chrono::steady_clock::now();
    double seconds_elapsed = std::chrono::duration<double>(current_time - last_collect_time_).count();
    
    // 获取网络接口列表
    std::vector<std::string> interfaces = getNetworkInterfaces();
    
    // 遍历所有接口，获取统计信息
    for (const auto& interface : interfaces) {
        std::map<std::string, unsigned long long> stats;
        
        if (getInterfaceStats(interface, stats)) {
            nlohmann::json interface_info;
            interface_info["interface"] = interface;
            
            // 设置基本统计信息
            interface_info["rx_bytes"] = stats["rx_bytes"];
            interface_info["tx_bytes"] = stats["tx_bytes"];
            interface_info["rx_packets"] = stats["rx_packets"];
            interface_info["tx_packets"] = stats["tx_packets"];
            interface_info["rx_errors"] = stats["rx_errors"];
            interface_info["tx_errors"] = stats["tx_errors"];
            
            // 如果有上次的统计信息，计算速率
            if (last_stats_.find(interface) != last_stats_.end() && seconds_elapsed > 0) {
                const auto& last = last_stats_[interface];
                
                // 计算速率（字节/秒）
                double rx_bytes_rate = (stats["rx_bytes"] - last.at("rx_bytes")) / seconds_elapsed;
                double tx_bytes_rate = (stats["tx_bytes"] - last.at("tx_bytes")) / seconds_elapsed;
                
                interface_info["rx_bytes_rate"] = rx_bytes_rate;
                interface_info["tx_bytes_rate"] = tx_bytes_rate;
            }
            
            // 更新上次的统计信息
            last_stats_[interface] = stats;
            
            result.push_back(interface_info);
        }
    }
    
    // 更新上次采集时间
    last_collect_time_ = current_time;
    
    return result;
}

std::string NetworkCollector::getType() const {
    return "network";
}

std::vector<std::string> NetworkCollector::getNetworkInterfaces() {
    std::vector<std::string> interfaces;
    
    // 打开/proc/net/dev文件
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) {
        return interfaces;
    }
    
    std::string line;
    // 跳过前两行（标题行）
    std::getline(file, line);
    std::getline(file, line);
    
    // 读取接口信息
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string interface_with_colon;
        iss >> interface_with_colon;
        
        // 移除接口名后的冒号
        std::string interface = interface_with_colon.substr(0, interface_with_colon.find(':'));
        
        // 去除前导空格
        interface.erase(0, interface.find_first_not_of(" \t"));
        
        // 过滤掉lo接口
        if (interface != "lo") {
            interfaces.push_back(interface);
        }
    }
    
    return interfaces;
}

bool NetworkCollector::getInterfaceStats(const std::string& interface, std::map<std::string, unsigned long long>& stats) {
    // 打开/proc/net/dev文件
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 查找指定接口的行
        if (line.find(interface + ":") != std::string::npos) {
            std::istringstream iss(line.substr(line.find(':') + 1));
            
            unsigned long long rx_bytes, rx_packets, rx_errors, rx_dropped, rx_fifo, rx_frame, rx_compressed, rx_multicast;
            unsigned long long tx_bytes, tx_packets, tx_errors, tx_dropped, tx_fifo, tx_collisions, tx_carrier, tx_compressed;
            
            iss >> rx_bytes >> rx_packets >> rx_errors >> rx_dropped >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast
                >> tx_bytes >> tx_packets >> tx_errors >> tx_dropped >> tx_fifo >> tx_collisions >> tx_carrier >> tx_compressed;
            
            stats["rx_bytes"] = rx_bytes;
            stats["rx_packets"] = rx_packets;
            stats["rx_errors"] = rx_errors;
            stats["tx_bytes"] = tx_bytes;
            stats["tx_packets"] = tx_packets;
            stats["tx_errors"] = tx_errors;
            
            return true;
        }
    }
    
    return false;
}
