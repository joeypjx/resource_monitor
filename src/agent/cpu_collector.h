#ifndef CPU_COLLECTOR_H
#define CPU_COLLECTOR_H

#include "resource_collector.h"


// @class CpuCollector
// @brief CPU资源采集器

// 继承自 `ResourceCollector`，专门负责采集与CPU相关的各项性能指标。
// - **CPU使用率**: 通过比较两次采集之间的CPU总时间和空闲时间来计算。
// - **系统负载**: 获取系统的1分钟、5分钟、15分钟平均负载。
// - **核心数**: 获取CPU的逻辑核心数量。

// 注意：CPU使用率的计算需要依赖上一次采集的状态（`last_total_time_` 和 `last_idle_time_`），
// 因此首次调用 `collect` 返回的使用率可能为0或不准确。

class CpuCollector : public ResourceCollector {
public:

// @brief 构造函数
// 初始化用于计算CPU使用率的成员变量。

    CpuCollector();
    

// @brief 默认的虚析构函数

    virtual ~CpuCollector() override = default;
    

// @brief 采集CPU资源信息
// 实现基类的纯虚函数。此方法会调用私有辅助函数来获取各项CPU指标。
// @return nlohmann::json 一个JSON对象，包含 'usage_percent', 'load_average', 'core_count' 等键。

    virtual nlohmann::json collect() override;
    

// @brief 获取采集器类型
// @return std::string 固定返回 "cpu"。

    virtual std::string getType() const override;

private:

// @brief 获取CPU的瞬时使用率
// 通过读取/proc/stat文件，比较当前与上一次采样点的CPU时间片差异来计算。
// @return double CPU使用率百分比 (0.0 - 100.0)。

    double getCpuUsagePercent();
    

// @brief 获取系统平均负载
// @param load_avg 一个大小为3的double数组，将用于存储1、5、15分钟的平均负载。
// @return bool 获取成功返回true。

    bool getLoadAverage(double load_avg[3]);
    

// @brief 获取CPU核心数
// @return int 逻辑CPU核心的数量。

    int getCoreCount();

private:
    // 这两个变量用于计算CPU使用率。通过记录上一次的CPU总时间和空闲时间，
    // 可以在下一次采样时计算出时间差内的CPU使用情况。
    unsigned long long last_total_time_;   ///< 上次采样时的总CPU时间（所有核心的jiffies总和）
    unsigned long long last_idle_time_;    ///< 上次采样时的CPU空闲时间（idle jiffies）
};

#endif // CPU_COLLECTOR_H
