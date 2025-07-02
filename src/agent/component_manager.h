#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

// 前向声明 (Forward declarations)
class DockerManager;    // 负责与Docker守护进程交互
class BinaryManager;    // 负责管理本地二进制文件和进程
class HttpClient;       // 用于与Manager服务通信

/**
 * @enum ComponentType
 * @brief 定义了组件支持的类型
 */
enum class ComponentType {
    DOCKER,   ///< 组件是一个Docker容器
    BINARY    ///< 组件是一个独立的二进制可执行文件
};

/**
 * @class ComponentManager
 * @brief 组件管理器
 *
 * 负责在Agent节点上管理业务组件的整个生命周期。
 * - **部署**: 根据Manager下发的指令，可以部署两种类型的组件：Docker容器或二进制程序。
 *   - 对于Docker组件，它会拉取镜像、创建和运行容器。
 *   - 对于二进制组件，它会下载可执行文件、创建配置文件并启动进程。
 * - **停止**: 停止正在运行的组件。
 * - **状态监控**: 启动一个后台线程，周期性地收集所有已部署组件的状态（如是否在运行、资源使用情况），
 *   并通过HttpClient将状态上报给Manager。
 *
 * 它聚合了DockerManager和BinaryManager，将具体的执行操作委托给它们。
 */
class ComponentManager {
public:
    /**
     * @brief 默认构造函数
     */
    ComponentManager() = default;
    
    /**
     * @brief 构造函数
     * @param http_client 指向HttpClient的共享指针，用于将组件状态上报给Manager。
     */
    explicit ComponentManager(std::shared_ptr<HttpClient> http_client);
    
    /**
     * @brief 析构函数
     * 自动停止状态收集线程。
     */
    ~ComponentManager();
    
    /**
     * @brief 初始化组件管理器
     * 创建内部的DockerManager和BinaryManager实例。
     * @return bool 初始化成功返回true。
     */
    bool initialize();

    // --- Component Lifecycle Management ---

    /**
     * @brief 将一个组件添加到管理器的监控列表
     * @param component_info 组件的详细信息JSON对象。
     */
    void addComponent(const nlohmann::json& component_info);
    
    /**
     * @brief 部署一个组件
     * 根据组件信息中的类型（DOCKER或BINARY）调用相应的内部部署方法。
     * @param component_info 组件的详细信息JSON对象。
     * @return nlohmann::json 包含部署结果（如容器ID或进程ID）的JSON对象。
     */
    nlohmann::json deployComponent(const nlohmann::json& component_info);
    
    /**
     * @brief 停止一个组件
     * @param component_info 包含要停止组件信息的JSON对象。
     * @return nlohmann::json 包含操作结果的JSON对象。
     */
    nlohmann::json stopComponent(const nlohmann::json& component_info);

    /**
     * @brief 从管理器的监控列表中移除一个组件
     * @param component_id 要移除的组件的唯一ID。
     * @return bool 移除成功返回true。
     */
    bool removeComponent(const std::string& component_id);

    // --- Status Collection & Reporting ---
    
    /**
     * @brief 启动周期性的状态收集和上报
     * @param interval_sec 收集和上报的时间间隔（秒）。
     * @return bool 成功启动返回true。
     */
    bool startStatusCollection(int interval_sec = 5);
    
    /**
     * @brief 停止状态收集和上报线程
     */
    void stopStatusCollection();

    /**
     * @brief 主动收集一次所有组件的状态
     * @return bool 收集成功返回true。
     */
    bool collectComponentStatus();

    /**
     * @brief 获取当前所有组件的状态
     * @return nlohmann::json 包含所有组件状态的JSON对象。
     */
    nlohmann::json getComponentStatus();

private:
    // --- Internal Deployment & Teardown Helpers ---

    /**
     * @brief 部署一个Docker类型的组件
     * @param component_info 组件的详细信息JSON对象。
     * @return nlohmann::json 部署结果。
     */
    nlohmann::json deployDockerComponent(const nlohmann::json& component_info);
    
    /**
     * @brief 部署一个二进制类型的组件
     * @param component_info 组件的详细信息JSON对象。
     * @return nlohmann::json 部署结果。
     */
    nlohmann::json deployBinaryComponent(const nlohmann::json& component_info);
    
    /**
     * @brief 停止一个Docker类型的组件
     * @param component_id 组件ID
     * @param container_id 容器ID
     * @return nlohmann::json 停止结果。
     */
    nlohmann::json stopDockerComponent(const std::string& component_id, 
                                     const std::string& container_id);
    
    /**
     * @brief 停止一个二进制类型的组件
     * @param component_id 组件ID
     * @param process_id 进程ID
     * @return nlohmann::json 停止结果。
     */
    nlohmann::json stopBinaryComponent(const std::string& component_id, 
                                     const std::string& process_id);
    
    // --- Internal Utility Helpers ---

    /**
     * @brief 下载Docker镜像
     * @param image_url 镜像的URL（如果需要从私有仓库下载）。
     * @param image_name 镜像的名称和标签。
     * @return nlohmann::json 下载结果。
     */
    nlohmann::json downloadImage(const std::string& image_url, const std::string& image_name);
    
    /**
     * @brief 下载二进制文件
     * @param binary_url 二进制文件的下载地址。
     * @param binary_path 本地保存路径。
     * @return nlohmann::json 下载结果。
     */
    nlohmann::json downloadBinary(const std::string& binary_url, const std::string& binary_path);
    
    /**
     * @brief 根据配置创建文件
     * @param config_files 包含文件路径和内容的JSON对象。
     * @return bool 创建成功返回true。
     */
    bool createConfigFiles(const nlohmann::json& config_files);
    
    /**
     * @brief 状态收集线程的主函数
     * 在一个循环中定期调用 collectComponentStatus。
     */
    void statusCollectionThread();

private:
    // --- Core Components ---
    std::shared_ptr<HttpClient> http_client_;        // HTTP客户端，用于上报状态
    std::unique_ptr<DockerManager> docker_manager_;  // Docker操作的实际执行者
    std::unique_ptr<BinaryManager> binary_manager_;  // 二进制文件和进程操作的实际执行者
    
    // --- State ---
    std::map<std::string, nlohmann::json> components_;  // 内存中维护的组件信息，key为组件ID
    std::mutex components_mutex_;                       // 用于保护对components_的并发访问
    
    // --- Threading for Status Collection ---
    std::atomic<bool> running_{false};               // 状态收集线程的运行标志
    int collection_interval_sec_;                    // 状态收集的时间间隔（秒）
    std::unique_ptr<std::thread> collection_thread_; // 指向状态收集线程的指针
};

#endif // COMPONENT_MANAGER_H
