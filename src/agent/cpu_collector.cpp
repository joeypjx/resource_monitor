#include "cpu_collector.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <unistd.h>

CpuCollector::CpuCollector() : last_total_time_(0), last_idle_time_(0) {
    // 初始化时先采集一次CPU时间，为计算使用率做准备
    getCpuUsagePercent();
}

nlohmann::json CpuCollector::collect() {
    nlohmann::json result = nlohmann::json::object();
    
    // 获取CPU使用率
    double usage_percent = getCpuUsagePercent();
    
    // 获取系统负载
    double load_avg[3] = {0.0, 0.0, 0.0};
    getLoadAverage(load_avg);
    
    // 获取CPU核心数
    int core_count = getCoreCount();
    
    // 构建JSON结果
    result["usage_percent"] = usage_percent;
    result["load_avg_1m"] = load_avg[0];
    result["load_avg_5m"] = load_avg[1];
    result["load_avg_15m"] = load_avg[2];
    result["core_count"] = core_count;
    
    return result;
}

std::string CpuCollector::getType() const {
    return "cpu";
}

double CpuCollector::getCpuUsagePercent() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return -1.0;
    }
    
    std::string line = "";
    if (!std::getline(file, line)) {
        return -1.0;
    }
    
    // 解析CPU时间
    std::istringstream iss(line);
    std::string cpu_label = "";
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0, guest = 0, guest_nice = 0;
    
    iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
    
    // 计算总时间和空闲时间
    unsigned long long idle_time = idle + iowait;
    unsigned long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    
    // 计算时间差
    unsigned long long total_time_delta = total_time - last_total_time_;
    unsigned long long idle_time_delta = idle_time - last_idle_time_;
    
    // 更新上次的时间
    last_total_time_ = total_time;
    last_idle_time_ = idle_time;
    
    // 如果是第一次采集，返回0
    if (total_time_delta == 0) {
        return 0.0;
    }
    
    // 计算CPU使用率
    double usage_percent = 100.0 * (1.0 - static_cast<double>(idle_time_delta) / static_cast<double>(total_time_delta));
    
    return usage_percent;
}

bool CpuCollector::getLoadAverage(double load_avg[3]) {
    if (getloadavg(load_avg, 3) != 3) {
        return false;
    }
    return true;
}

int CpuCollector::getCoreCount() {
    return std::thread::hardware_concurrency();
}
