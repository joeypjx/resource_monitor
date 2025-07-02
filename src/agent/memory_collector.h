#ifndef MEMORY_COLLECTOR_H
#define MEMORY_COLLECTOR_H

#include "resource_collector.h"

/**
 * @class MemoryCollector
 * @brief 内存资源采集器
 *
 * 继承自 `ResourceCollector`，专门负责采集与物理内存（RAM）相关的各项性能指标。
 * 它主要通过解析Linux系统下的 `/proc/meminfo` 文件来获取信息。
 *
 * 采集的指标包括：
 * - **总内存 (total)**: 系统中的物理内存总量。
 * - **空闲内存 (free)**: 完全未被使用的内存。
 * - **可用内存 (available)**: 可供应用程序使用的内存，包括空闲内存和可回收的缓存/缓冲区内存。
 *   这个指标通常比 `free` 更能反映实际可用的内存情况。
 */
class MemoryCollector : public ResourceCollector {
public:
    /**
     * @brief 构造函数
     */
    MemoryCollector() = default;
    
    /**
     * @brief 默认的虚析构函数
     */
    virtual ~MemoryCollector() override = default;
    
    /**
     * @brief 采集内存资源信息
     * 实现基类的纯虚函数。此方法调用 `getMemoryInfo` 来获取内存指标。
     * @return nlohmann::json 一个JSON对象，包含 'total_kb', 'free_kb', 'available_kb', 'usage_percent' 等键。
     */
    virtual nlohmann::json collect() override;
    
    /**
     * @brief 获取采集器类型
     * @return std::string 固定返回 "memory"。
     */
    virtual std::string getType() const override;

private:
    /**
     * @brief 解析/proc/meminfo文件以获取核心内存信息
     * 
     * @param[out] total 用于存储总内存大小(KB)的变量的引用。
     * @param[out] free 用于存储空闲内存大小(KB)的变量的引用。
     * @param[out] available 用于存储可用内存大小(KB)的变量的引用。
     * @return bool 如果成功解析到所有需要的字段，则返回true。
     */
    bool getMemoryInfo(unsigned long long& total, unsigned long long& free, unsigned long long& available);
};

#endif // MEMORY_COLLECTOR_H
