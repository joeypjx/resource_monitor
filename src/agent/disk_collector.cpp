#include "disk_collector.h"
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>
#include <mntent.h>

DiskCollector::DiskCollector() {
    // 构造函数中获取磁盘分区信息
    getDiskPartitions();
}

nlohmann::json DiskCollector::collect() {
    nlohmann::json result = nlohmann::json::array();
    
    // 遍历所有分区，获取使用情况
    for (const auto& partition : partitions_) {
        unsigned long long total = 0, used = 0, free = 0;
        
        if (getDiskUsage(partition.mount_point, total, used, free)) {
            double usage_percent = 0.0;
            if (total > 0) {
                usage_percent = 100.0 * static_cast<double>(used) / total;
            }
            
            nlohmann::json disk_info;
            disk_info["device"] = partition.device;
            disk_info["mount_point"] = partition.mount_point;
            disk_info["total"] = total * 1024;  // 转换为字节
            disk_info["used"] = used * 1024;    // 转换为字节
            disk_info["free"] = free * 1024;    // 转换为字节
            disk_info["usage_percent"] = usage_percent;
            
            result.push_back(disk_info);
        }
    }
    
    return result;
}

std::string DiskCollector::getType() const {
    return "disk";
}

bool DiskCollector::getDiskPartitions() {
    FILE* mtab = setmntent("/etc/mtab", "r");
    if (!mtab) {
        return false;
    }
    
    struct mntent* mnt;
    while ((mnt = getmntent(mtab)) != nullptr) {
        // 过滤掉不需要的文件系统类型
        if (std::string(mnt->mnt_type) == "proc" ||
            std::string(mnt->mnt_type) == "sysfs" ||
            std::string(mnt->mnt_type) == "devtmpfs" ||
            std::string(mnt->mnt_type) == "devpts" ||
            std::string(mnt->mnt_type) == "tmpfs" ||
            std::string(mnt->mnt_type) == "cgroup" ||
            std::string(mnt->mnt_type) == "pstore" ||
            std::string(mnt->mnt_type) == "securityfs" ||
            std::string(mnt->mnt_type) == "debugfs") {
            continue;
        }
        
        DiskPartition partition;
        partition.device = mnt->mnt_fsname;
        partition.mount_point = mnt->mnt_dir;
        partition.fs_type = mnt->mnt_type;
        
        partitions_.push_back(partition);
    }
    
    endmntent(mtab);
    return true;
}

bool DiskCollector::getDiskUsage(const std::string& mount_point, 
                                unsigned long long& total, 
                                unsigned long long& used, 
                                unsigned long long& free) {
    struct statvfs stat;
    if (statvfs(mount_point.c_str(), &stat) != 0) {
        return false;
    }
    
    // 计算总容量、已用容量和空闲容量（单位：KB）
    total = (stat.f_blocks * stat.f_frsize) / 1024;
    free = (stat.f_bfree * stat.f_frsize) / 1024;
    used = total - free;
    
    return true;
}
