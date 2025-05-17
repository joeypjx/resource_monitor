#ifndef DISK_COLLECTOR_H
#define DISK_COLLECTOR_H

#include "resource_collector.h"
#include <vector>
#include <string>

/**
 * DiskCollector类 - 磁盘资源采集器
 * 
 * 负责采集磁盘相关资源信息
 */
class DiskCollector : public ResourceCollector {
public:
    /**
     * 构造函数
     */
    DiskCollector();
    
    /**
     * 析构函数
     */
    ~DiskCollector() override = default;
    
    /**
     * 采集磁盘资源信息
     * 
     * @return JSON格式的磁盘资源信息
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
     * 获取磁盘分区信息
     * 
     * @return 是否成功获取
     */
    bool getDiskPartitions();

    /**
     * 获取指定分区的使用情况
     * 
     * @param mount_point 挂载点
     * @param total 总容量(KB)
     * @param used 已用容量(KB)
     * @param free 空闲容量(KB)
     * @return 是否成功获取
     */
    bool getDiskUsage(const std::string& mount_point, 
                      unsigned long long& total, 
                      unsigned long long& used, 
                      unsigned long long& free);

private:
    struct DiskPartition {
        std::string device;
        std::string mount_point;
        std::string fs_type;
    };
    
    std::vector<DiskPartition> partitions_;
};

#endif // DISK_COLLECTOR_H
