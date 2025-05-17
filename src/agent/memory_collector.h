#ifndef MEMORY_COLLECTOR_H
#define MEMORY_COLLECTOR_H

#include "resource_collector.h"

/**
 * MemoryCollector类 - 内存资源采集器
 * 
 * 负责采集内存相关资源信息
 */
class MemoryCollector : public ResourceCollector {
public:
    /**
     * 构造函数
     */
    MemoryCollector();
    
    /**
     * 析构函数
     */
    ~MemoryCollector() override = default;
    
    /**
     * 采集内存资源信息
     * 
     * @return JSON格式的内存资源信息
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
     * 解析/proc/meminfo文件获取内存信息
     * 
     * @param total 总内存大小(KB)
     * @param free 空闲内存大小(KB)
     * @param available 可用内存大小(KB)
     * @return 是否成功获取
     */
    bool getMemoryInfo(unsigned long long& total, unsigned long long& free, unsigned long long& available);
};

#endif // MEMORY_COLLECTOR_H
