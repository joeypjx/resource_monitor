#ifndef RESOURCE_COLLECTOR_H
#define RESOURCE_COLLECTOR_H

#include <string>
#include <nlohmann/json.hpp>

// @class ResourceCollector
// @brief 资源采集器抽象基类 (Abstract Base Class for Resource Collectors)

// 定义了所有具体资源采集器（如CpuCollector, MemoryCollector）必须实现的通用接口。
// 这种设计遵循了策略模式，允许Agent在运行时动态地处理不同类型的资源采集任务。
// 每个派生类负责采集一种特定类型的资源（例如CPU使用率、内存使用情况）。
class ResourceCollector {
public:
    // @brief 虚析构函数
    // 确保通过基类指针删除派生类对象时，能够正确调用派生类的析构函数。
    virtual ~ResourceCollector() = default;
    
    // @brief 纯虚函数，用于采集资源信息
    // 派生类必须实现此方法，以返回一个包含特定资源指标的JSON对象。
    // @return nlohmann::json 采集到的资源数据。
    //         例如: `{"usage": 10.5, "cores": 4}`
    virtual nlohmann::json collect() = 0;
    
    // @brief 纯虚函数，用于获取采集器的类型
    // 派生类必须实现此方法，以返回一个标识其类型的字符串。
    // @return std::string 采集器的类型名称 (e.g., "cpu", "memory")。
    virtual std::string getType() const = 0;
};

#endif // RESOURCE_COLLECTOR_H
