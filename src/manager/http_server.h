#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

// 前向声明
class DatabaseManager;
class BusinessManager;

/**
 * HttpServer类 - HTTP服务器
 * 
 * 负责处理HTTP请求，提供API接口
 */
class HttpServer {
public:
    /**
     * 构造函数
     * 
     * @param port 监听端口
     * @param db_manager 数据库管理器
     * @param business_manager 业务管理器
     */
    HttpServer(int port, std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<BusinessManager> business_manager);
    
    /**
     * 析构函数
     */
    ~HttpServer();
    
    /**
     * 启动HTTP服务器
     * 
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * 停止HTTP服务器
     */
    void stop();

private:
    // 资源监控API处理函数
    nlohmann::json handleAgentRegistration(const nlohmann::json& request);
    nlohmann::json handleResourceReport(const nlohmann::json& request);
    nlohmann::json handleGetAgents();
    nlohmann::json handleGetAgentResources(const std::string& agent_id, const std::string& resource_type, int limit);
    
    // 业务部署API处理函数
    nlohmann::json handleDeployBusiness(const nlohmann::json& request);
    nlohmann::json handleStopBusiness(const std::string& business_id);
    nlohmann::json handleRestartBusiness(const std::string& business_id);
    nlohmann::json handleUpdateBusiness(const std::string& business_id, const nlohmann::json& request);
    nlohmann::json handleGetBusinesses();
    nlohmann::json handleGetBusinessDetails(const std::string& business_id);
    nlohmann::json handleGetBusinessComponents(const std::string& business_id);
    nlohmann::json handleComponentStatusReport(const nlohmann::json& request);

private:
    int port_;                                      // 监听端口
    std::shared_ptr<DatabaseManager> db_manager_;   // 数据库管理器
    std::shared_ptr<BusinessManager> business_manager_; // 业务管理器
    bool running_;                                  // 运行标志
    void* server_;                                  // HTTP服务器实例
};

#endif // HTTP_SERVER_H
