#ifndef RESOURCE_MONITOR_AGENT_H
#define RESOURCE_MONITOR_AGENT_H

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

// 前向声明
class ResourceCollector;
class HttpClient;
class ComponentManager;
class NodeController;

namespace httplib {
    class Server;
}

/**
 * Agent类 - 资源监控代理
 * 
 * 负责在工作节点上采集资源信息并上报给Manager
 */
class Agent {
public:
    /**
     * 构造函数
     * 
     * @param manager_url Manager的URL地址
     * @param hostname 主机名
     * @param collection_interval_sec 资源采集间隔（秒）
     */
    Agent(const std::string& manager_url, 
          const std::string& hostname = "",
          int collection_interval_sec = 5);
    
    /**
     * 析构函数
     */
    ~Agent();
    
    /**
     * 启动Agent
     * 
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * 停止Agent
     */
    void stop();
    
    /**
     * 获取Board ID
     * 
     * @return Board唯一标识符
     */
    std::string getAgentId() const;

    void sendHeartbeat();

private:
    /**
     * 向Manager注册
     * 
     * @return 是否注册成功
     */
    bool registerToManager();
    
    /**
     * 采集并上报资源信息
     */
    void collectAndReportResources();
    
    /**
     * 工作线程函数
     */
    void workerThread();

    void getLocalIpAddress();

    void getOsInfo();

    /**
     * 启动HTTP服务器
     * 
     * @param port HTTP服务器端口
     * @return 是否成功启动
     */
    bool startHttpServer(int port = 8081);
    
    /**
     * 处理组件部署请求
     * 
     * @param request 请求内容
     * @return 响应内容
     */
    nlohmann::json handleDeployRequest(const nlohmann::json& request);
    
    /**
     * 处理组件停止请求
     * 
     * @param request 请求内容
     * @return 响应内容
     */
    nlohmann::json handleStopRequest(const nlohmann::json& request);

    // 新增：处理节点控制请求
    nlohmann::json handleNodeControlRequest(const nlohmann::json& request);

    void heartbeatThread();

private:
    std::string manager_url_;                      // Manager的URL地址
    std::string hostname_;                         // 主机名
    std::string agent_id_;                         // Board唯一标识符
    std::string ip_address_;                       // IP地址
    std::string os_info_;                          // 操作系统信息
    
    int collection_interval_sec_;                  // 资源采集间隔（秒）
    std::atomic<bool> running_;                    // 运行标志
    
    std::shared_ptr<HttpClient> http_client_;      // HTTP客户端
    std::vector<std::unique_ptr<ResourceCollector>> collectors_;  // 资源采集器集合
    std::shared_ptr<ComponentManager> component_manager_; // 组件管理器
    
    std::thread worker_thread_;                    // 工作线程
    std::thread heartbeat_thread_;
    bool heartbeat_running_ = false;
    
    httplib::Server* http_server_;                 // HTTP服务器
    std::atomic<bool> server_running_;             // 服务器运行标志

    std::shared_ptr<NodeController> node_controller_;
};

#endif // RESOURCE_MONITOR_AGENT_H
