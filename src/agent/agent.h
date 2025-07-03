#ifndef RESOURCE_MONITOR_AGENT_H
#define RESOURCE_MONITOR_AGENT_H

#include <string>           // for std::string
#include <memory>           // for std::unique_ptr, std::shared_ptr
#include <chrono>           // for std::chrono
#include <thread>           // for std::thread
#include <atomic>           // for std::atomic
#include <vector>           // for std::vector
#include <functional>       // for std::function
#include <nlohmann/json.hpp> // for JSON processing

// 前向声明 (Forward declarations)
// 避免在头文件中引入完整的类定义，减少编译依赖
class ResourceCollector;    // 资源收集器基类
class HttpClient;           // HTTP客户端，用于与Manager通信
class ComponentManager;     // 组件管理器，负责部署和管理组件
class NodeController;       // 节点控制器，提供对节点的控制接口（暂未使用）

namespace httplib {
    class Server;           // HTTP服务器，用于接收Manager的指令
}

// @class Agent
// @brief 资源监控代理 (Resource Monitoring Agent)
// 
// Agent类是运行在被监控节点上的核心。它负责：
// 1. 周期性地采集节点的硬件资源信息（CPU, 内存等）。
// 2. 将采集到的数据上报给中心Manager。
// 3. 接收并执行Manager下发的指令，如部署、停止组件。
// 4. 维护一个本地的HTTP服务，用于与Manager进行双向通信。
class Agent {
public:

// @brief Agent 构造函数
// 
// @param manager_url Manager的URL地址, 用于上报数据和注册
// @param hostname 自定义的主机名。如果为空，将自动获取。
// @param collection_interval_sec 资源采集的时间间隔（秒）
// @param port Agent本地HTTP服务器的端口
// @param network_interface 指定用于获取IP地址的网络接口名称。如果为空，将自动检测。

    Agent(const std::string& manager_url, 
          const std::string& hostname = "",
          int collection_interval_sec = 5,
          int port = 8081,
          const std::string& network_interface = "");
    

// @brief 析构函数
// 自动停止Agent并清理资源。

    ~Agent();
    

// @brief 启动Agent
// 初始化所有组件，启动工作线程和HTTP服务器。
// @return bool 如果成功启动返回true, 否则返回false。

    bool start();
    

// @brief 停止Agent
// 停止所有正在运行的线程和服务。

    void stop();
    

// @brief 初始化Agent
// 负责准备Agent运行所需的所有资源和信息。
// 此函数在 `start()` 内部被调用。

    void init();

private:

// @brief 向Manager注册
// Agent启动时调用此函数，将自身信息注册到Manager，并获取一个唯一的Agent ID。
// @return bool 如果注册成功返回true, 否则返回false。

    bool registerToManager();
    

// @brief 采集并上报资源信息
// 由工作线程周期性调用，采集各类资源信息并发送给Manager。

    void collectAndReportResources();
    

// @brief 工作线程函数
// Agent的核心循环所在，周期性地执行 `collectAndReportResources`。

    void workerThread();


// @brief 获取本机主机名
// @return std::string 主机名

    std::string getHostname();


// @brief 获取本机IP地址
// @return std::string IP地址

    std::string getLocalIpAddress();


// @brief 获取操作系统信息
// @return std::string 操作系统信息

    std::string getOsInfo();


// @brief 获取CPU型号
// @return std::string CPU型号

    std::string getCpuModel();


// @brief 获取GPU数量
// @return int GPU数量

    int getGpuCount();
    

// @brief 启动HTTP服务器
// 初始化并运行一个后台HTTP服务，用于接收来自Manager的命令。
// @param port 要监听的HTTP服务器端口
// @return bool 如果成功启动返回true, 否则返回false。

    bool startHttpServer(int port);
    

// @brief 处理组件部署请求
// 当Manager通过HTTP POST请求 `/deploy` 时被调用。
// @param request 包含部署信息的JSON对象
// @return nlohmann::json 包含执行结果的响应JSON对象

    nlohmann::json handleDeployRequest(const nlohmann::json& request);
    

// @brief 处理组件停止请求
// 当Manager通过HTTP POST请求 `/stop` 时被调用。
// @param request 包含停止信息的JSON对象
// @return nlohmann::json 包含执行结果的响应JSON对象

    nlohmann::json handleStopRequest(const nlohmann::json& request);


// @brief 从文件中读取Agent ID
// @param file_path 存储Agent ID的文件路径
// @return std::string Agent ID，如果文件不存在或读取失败则返回空字符串

    std::string readAgentIdFromFile(const std::string& file_path);


// @brief 将Agent ID写入文件
// @param file_path 存储Agent ID的文件路径
// @param id 要写入的Agent ID

    void writeAgentIdToFile(const std::string& file_path, const std::string& id);

private:
    // --- 配置信息 ---
    std::string manager_url_;           // Manager的URL地址
    std::string hostname_;              // 主机名
    int collection_interval_sec_;       // 资源采集间隔（秒）
    int port_;                          // Agent本地HTTP服务器端口
    std::string network_interface_;     // 用于获取IP的网络接口名称

    // --- 状态信息 ---
    std::string agent_id_;              // 由Manager分配的唯一Agent ID
    std::string ip_address_;            // 本机IP地址
    std::string os_info_;               // 操作系统信息
    std::string cpu_model_;             // CPU型号
    int gpu_count_;                     // GPU数量
    std::atomic<bool> running_;         // Agent是否正在运行的标志
    
    // --- 核心组件 ---
    std::shared_ptr<HttpClient> http_client_;           // HTTP客户端，负责与Manager通信
    std::vector<std::unique_ptr<ResourceCollector>> collectors_; // 资源采集器列表 (CPU, Memory等)
    std::shared_ptr<ComponentManager> component_manager_; // 组件管理器，负责组件的生命周期
    
    // --- 线程与服务 ---
    std::thread worker_thread_;         // 执行周期性任务的工作线程
    httplib::Server* http_server_;      // 指向HTTP服务器实例的裸指针
    std::atomic<bool> server_running_;  // HTTP服务器是否正在运行的标志

    // --- 暂未使用 ---
    std::shared_ptr<NodeController> node_controller_;
};

#endif // RESOURCE_MONITOR_AGENT_H
