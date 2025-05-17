#ifndef RESOURCE_COLLECTOR_H
#define RESOURCE_COLLECTOR_H

#include <string>
#include <nlohmann/json.hpp>

/**
 * ResourceCollector抽象基类 - 资源采集器
 * 
 * 定义资源采集的通用接口
 */
class ResourceCollector {
public:
    /**
     * 析构函数
     */
    virtual ~ResourceCollector() = default;
    
    /**
     * 采集资源信息
     * 
     * @return JSON格式的资源信息
     */
    virtual nlohmann::json collect() = 0;
    
    /**
     * 获取采集器类型
     * 
     * @return 采集器类型名称
     */
    virtual std::string getType() const = 0;
};

#endif // RESOURCE_COLLECTOR_H
