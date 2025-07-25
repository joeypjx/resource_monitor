#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

// 前向声明
class DockerManager;
class BinaryManager;
class HttpClient;

/**
 * 组件类型枚举
 */
enum class ComponentType {
    DOCKER,   // Docker容器类型
    BINARY    // 二进制运行体类型
};

/**
 * ComponentManager类 - 组件管理器
 * 
 * 负责管理业务组件的生命周期，包括部署、停止和状态收集
 */
class ComponentManager {
public:
    /**
     * 构造函数
     * 
     * @param http_client HTTP客户端，用于与Manager通信
     */
    explicit ComponentManager(std::shared_ptr<HttpClient> http_client);
    
    /**
     * 析构函数
     */
    ~ComponentManager();
    
    /**
     * 初始化组件管理器
     * 
     * @return 是否成功初始化
     */
    bool initialize();

    /**
     * 添加组件
     * 
     * @param component_info 组件信息
     */
    void addComponent(const nlohmann::json& component_info);
    
    /**
     * 部署组件
     * 
     * @param component_info 组件信息
     * @return 部署结果
     */
    nlohmann::json deployComponent(const nlohmann::json& component_info);
    
    /**
     * 停止组件
     * 
     * @param component_id 组件ID
     * @param business_id 业务ID
     * @param container_or_process_id 容器ID或进程ID
     * @param component_type 组件类型
     * @return 停止结果
     */
    nlohmann::json stopComponent(const nlohmann::json& component_info);
    
    /**
     * 收集组件状态
     * 
     * @return 是否成功收集
     */
    bool collectComponentStatus();
    
    /**
     * 启动状态收集和上报
     * 
     * @param interval_sec 收集间隔（秒）
     * @return 是否成功启动
     */
    bool startStatusCollection(int interval_sec = 5);
    
    /**
     * 停止状态收集和上报
     */
    void stopStatusCollection();

    /**
     * 获取组件状态
     * 
     * @return 组件状态
     */
    nlohmann::json getComponentStatus();

    /**
     * 移除组件
     * 
     * @param component_id 组件ID
     * @return 是否成功移除
     */
    bool removeComponent(const std::string& component_id);

private:
    /**
     * 部署Docker容器组件
     * 
     * @param component_info 组件信息
     * @return 部署结果
     */
    nlohmann::json deployDockerComponent(const nlohmann::json& component_info);
    
    /**
     * 部署二进制运行体组件
     * 
     * @param component_info 组件信息
     * @return 部署结果
     */
    nlohmann::json deployBinaryComponent(const nlohmann::json& component_info);
    
    /**
     * 停止Docker容器组件
     * 
     * @param component_id 组件ID
     * @param business_id 业务ID
     * @param container_id 容器ID
     * @return 停止结果
     */
    nlohmann::json stopDockerComponent(const std::string& component_id, 
                                     const std::string& business_id, 
                                     const std::string& container_id);
    
    /**
     * 停止二进制运行体组件
     * 
     * @param component_id 组件ID
     * @param business_id 业务ID
     * @param process_id 进程ID
     * @return 停止结果
     */
    nlohmann::json stopBinaryComponent(const std::string& component_id, 
                                     const std::string& business_id, 
                                     const std::string& process_id);
    
    /**
     * 下载Docker镜像
     * 
     * @param image_url 镜像URL
     * @param image_name 镜像名称
     * @return 下载结果
     */
    nlohmann::json downloadImage(const std::string& image_url, const std::string& image_name);
    
    /**
     * 下载二进制文件
     * 
     * @param binary_url 二进制文件URL
     * @param binary_path 保存路径
     * @return 下载结果
     */
    nlohmann::json downloadBinary(const std::string& binary_url, const std::string& binary_path);
    
    /**
     * 创建配置文件
     * 
     * @param config_files 配置文件列表
     * @return 是否成功创建
     */
    bool createConfigFiles(const nlohmann::json& config_files);
    
    /**
     * 状态收集线程函数
     */
    void statusCollectionThread();

private:
    std::shared_ptr<HttpClient> http_client_;        // HTTP客户端
    std::unique_ptr<DockerManager> docker_manager_;  // Docker管理器
    std::unique_ptr<BinaryManager> binary_manager_;  // 二进制运行体管理器
    
    std::map<std::string, nlohmann::json> components_;  // 组件信息，key为组件ID
    std::mutex components_mutex_;                       // 组件信息互斥锁
    
    bool running_;                                   // 状态收集线程运行标志
    std::unique_ptr<std::thread> collection_thread_; // 状态收集线程
    int collection_interval_sec_;                    // 状态收集间隔（秒）
};

#endif // COMPONENT_MANAGER_H
