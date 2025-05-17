#ifndef NETWORK_COLLECTOR_H
#define NETWORK_COLLECTOR_H

#include "resource_collector.h"
#include <vector>
#include <string>
#include <map>
#include <chrono>

/**
 * NetworkCollector类 - 网络资源采集器
 * 
 * 负责采集网络相关资源信息
 */
class NetworkCollector : public ResourceCollector {
public:
    /**
     * 构造函数
     */
    NetworkCollector();
    
    /**
     * 析构函数
     */
    ~NetworkCollector() override = default;
    
    /**
     * 采集网络资源信息
     * 
     * @return JSON格式的网络资源信息
     */
    nlohmann::json collect() override;
    
    /**
     * 获取采集器类型
     * 
     * @return 采集器类型名称
     */
    std::string getType() const override;

private:
    /**
     * 获取网络接口列表
     * 
     * @return 网络接口名称列表
     */
    std::vector<std::string> getNetworkInterfaces();
    
    /**
     * 获取网络接口统计信息
     * 
     * @param interface 接口名称
     * @param stats 存储统计信息的映射
     * @return 是否成功获取
     */
    bool getInterfaceStats(const std::string& interface, std::map<std::string, unsigned long long>& stats);

private:
    // 上次采集的网络接口统计信息，用于计算速率
    std::map<std::string, std::map<std::string, unsigned long long>> last_stats_;
    // 上次采集的时间戳
    std::chrono::steady_clock::time_point last_collect_time_;
};

#endif // NETWORK_COLLECTOR_H
