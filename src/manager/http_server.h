#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <httplib.h>
#include <nlohmann/json.hpp>

// 前向声明
class DatabaseManager;
class BusinessManager;

/**
 * HTTPServer类 - HTTP服务器
 * 
 * 提供HTTP API接口
 */
class HTTPServer {
public:
    /**
     * 构造函数
     * 
     * @param db_manager 数据库管理器
     * @param business_manager 业务管理器
     * @param port 监听端口
     */
    HTTPServer(std::shared_ptr<DatabaseManager> db_manager, 
              std::shared_ptr<BusinessManager> business_manager,
              int port = 8080);
    
    /**
     * 析构函数
     */
    ~HTTPServer();
    
    /**
     * 启动服务器
     * 
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * 停止服务器
     */
    void stop();

private:
    /**
     * 初始化路由
     */
    void initRoutes();
    
    /**
     * 处理节点注册
     */
    void handleNodeRegistration(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理资源上报
     */
    void handleResourceReport(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取节点列表
     */
    void handleGetNodes(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取节点详情
     */
    void handleGetNodeDetails(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取节点资源历史
     */
    void handleGetNodeResourceHistory(const httplib::Request& req, httplib::Response& res);

    /**
     * 处理获取节点资源
     */
    void handleGetAgentResources(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理业务部署
     */
    void handleDeployBusiness(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理业务停止
     */
    void handleStopBusiness(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理业务重启
     */
    void handleRestartBusiness(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理业务更新
     */
    void handleUpdateBusiness(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务列表
     */
    void handleGetBusinesses(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务详情
     */
    void handleGetBusinessDetails(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务组件
     */
    void handleGetBusinessComponents(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理组件状态上报
     */
    void handleComponentStatusReport(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理创建组件模板
     */
    void handleCreateComponentTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取组件模板列表
     */
    void handleGetComponentTemplates(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取组件模板详情
     */
    void handleGetComponentTemplateDetails(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理更新组件模板
     */
    void handleUpdateComponentTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理删除组件模板
     */
    void handleDeleteComponentTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理创建业务模板
     */
    void handleCreateBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务模板列表
     */
    void handleGetBusinessTemplates(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务模板详情
     */
    void handleGetBusinessTemplateDetails(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理更新业务模板
     */
    void handleUpdateBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理删除业务模板
     */
    void handleDeleteBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    
    /**
     * 处理获取业务模板作为业务
     */
    void handleGetBusinessTemplateAsBusiness(const httplib::Request& req, httplib::Response& res);

    /**
     * 处理获取集群指标
     */
    void handleGetClusterMetrics(const httplib::Request& req, httplib::Response& res);

    /**
     * 处理获取集群指标历史
     */
    void handleGetClusterMetricsHistory(const httplib::Request& req, httplib::Response& res);

private:
    std::shared_ptr<DatabaseManager> db_manager_;    // 数据库管理器
    std::shared_ptr<BusinessManager> business_manager_;  // 业务管理器
    int port_;  // 监听端口
    httplib::Server server_;  // HTTP服务器
    bool running_;  // 是否正在运行
};

#endif // HTTP_SERVER_H
