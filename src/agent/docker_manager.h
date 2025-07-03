#ifndef DOCKER_MANAGER_H
#define DOCKER_MANAGER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>


// DockerManager类 - Docker容器管理器
// 
// 负责与Docker守护进程交互，管理容器的生命周期

class DockerManager {
public:

// 构造函数

    DockerManager();
    

// 析构函数

    ~DockerManager() = default;
    

// 初始化Docker管理器
// 
// @return 是否成功初始化

    bool initialize();
    

// 检查Docker守护进程是否可用
// 
// @return 是否可用

    bool checkDockerAvailable();
    

// 下载Docker镜像
// 
// @param image_url 镜像URL
// @param image_name 镜像名称
// @return 下载结果

    nlohmann::json pullImage(const std::string& image_url, const std::string& image_name);
    

// 创建并启动容器
// 
// @param image_name 镜像名称
// @param container_name 容器名称
// @param env_vars 环境变量
// @param resource_limits 资源限制
// @param volumes 挂载卷
// @return 创建结果

    nlohmann::json createContainer(const std::string& image_name,
                                 const std::string& container_name,
                                 const nlohmann::json& env_vars,
                                 const nlohmann::json& resource_limits,
                                 const std::vector<std::string>& volumes);
    

// 停止容器
// 
// @param container_id 容器ID
// @return 停止结果

    nlohmann::json stopContainer(const std::string& container_id);
    

// 删除容器
// 
// @param container_id 容器ID
// @return 删除结果

    nlohmann::json removeContainer(const std::string& container_id);
    

// 获取容器状态
// 
// @param container_id 容器ID
// @return 容器状态

    nlohmann::json getContainerStatus(const std::string& container_id);
    

// 获取容器资源使用情况
// 
// @param container_id 容器ID
// @return 资源使用情况

    nlohmann::json getContainerStats(const std::string& container_id);
    

// 获取所有容器列表
// 
// @param all 是否包含已停止的容器
// @return 容器列表

    nlohmann::json listContainers(bool all = false);

private:

// 执行Docker命令
// 
// @param command 命令
// @return 命令输出

    std::string executeDockerCommand(const std::string& command);
    

// 通过Docker API执行请求
// 
// @param method HTTP方法
// @param endpoint API端点
// @param request_body 请求体
// @return 响应

    nlohmann::json dockerApiRequest(const std::string& method, 
                                  const std::string& endpoint, 
                                  const nlohmann::json& request_body = nlohmann::json());

private:
    std::string docker_socket_path_;  // Docker套接字路径
    bool use_api_;                    // 是否使用Docker API
};

#endif // DOCKER_MANAGER_H
