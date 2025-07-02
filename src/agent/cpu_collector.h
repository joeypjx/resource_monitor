#ifndef CPU_COLLECTOR_H
#define CPU_COLLECTOR_H

#include "resource_collector.h"

/**
 * CpuCollector类 - CPU资源采集器
 * 
 * 负责采集CPU相关资源信息
 */
class CpuCollector : public ResourceCollector {
public:
    /**
     * 构造函数
     */
    CpuCollector();
    
    /**
     * 析构函数
     */
    virtual ~CpuCollector() override = default;
    
    /**
     * 采集CPU资源信息
     * 
     * @return JSON格式的CPU资源信息
     */
    virtual nlohmann::json collect() override;
    
    /**
     * 获取采集器类型
     * 
     * @return 采集器类型名称
     */
    virtual std::string getType() const override;

private:
    /**
     * 获取CPU使用率
     * 
     * @return CPU使用率百分比
     */
    double getCpuUsagePercent();
    
    /**
     * 获取系统负载
     * 
     * @param load_avg 存储1分钟、5分钟、15分钟负载的数组
     * @return 是否成功获取
     */
    bool getLoadAverage(double load_avg[3]);
    
    /**
     * 获取CPU核心数
     * 
     * @return CPU核心数
     */
    int getCoreCount();

private:
    unsigned long long last_total_time_;   // 上次采集的总CPU时间
    unsigned long long last_idle_time_;    // 上次采集的空闲CPU时间
};

#endif // CPU_COLLECTOR_H
