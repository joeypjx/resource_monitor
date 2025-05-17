#ifndef DOCKER_COLLECTOR_H
#define DOCKER_COLLECTOR_H

#include "resource_collector.h"
#include <vector>
#include <string>

/**
 * DockerCollector类 - Docker资源采集器
 * 
 * 负责采集Docker容器相关资源信息
 */
class DockerCollector : public ResourceCollector {
public:
    /**
     * 构造函数
     * 
     * @param docker_socket Docker套接字路径，默认为"/var/run/docker.sock"
     */
    explicit DockerCollector(const std::string& docker_socket = "/var/run/docker.sock");
    
    /**
     * 析构函数
     */
    ~DockerCollector() override = default;
    
    /**
     * 采集Docker资源信息
     * 
     * @return JSON格式的Docker资源信息
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
     * 检查Docker服务是否可用
     * 
     * @return 是否可用
     */
    bool isDockerAvailable();
    
    /**
     * 获取容器列表
     * 
     * @return 容器信息列表
     */
    nlohmann::json getContainers();
    
    /**
     * 获取容器统计信息
     * 
     * @param container_id 容器ID
     * @return 容器统计信息
     */
    nlohmann::json getContainerStats(const std::string& container_id);
    
    /**
     * 发送HTTP请求到Docker API
     * 
     * @param endpoint API端点
     * @param method HTTP方法
     * @return 响应内容
     */
    std::string sendDockerApiRequest(const std::string& endpoint, const std::string& method = "GET");

private:
    std::string docker_socket_;  // Docker套接字路径
    bool docker_available_;      // Docker服务是否可用
};

#endif // DOCKER_COLLECTOR_H
