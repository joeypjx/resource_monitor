#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <httplib.h>
#include <nlohmann/json.hpp>

// 前向声明 (Forward declarations)
class DatabaseManager;
class BusinessManager;

// @class HTTPServer
// @brief HTTP服务器

// 使用 `cpp-httplib` 库实现，作为Manager的API网关。
// - **路由注册**: 在初始化时，注册所有API端点，并将它们与处理函数绑定。
// - **请求处理**: 接收HTTP请求，解析参数和请求体，然后调用BusinessManager或DatabaseManager中相应的方法来处理请求。
// - **响应生成**: 将处理结果（无论是成功数据还是错误信息）封装成统一的JSON格式并返回给客户端。
// - **静态文件服务**: 托管Web前端静态资源（HTML, CSS, JS）。
class HTTPServer {
public:
    // --- Constructor & Lifecycle ---
    
    // @brief 默认构造函数
    HTTPServer() = default;

    // @brief 构造函数
    // @param db_manager 指向数据库管理器的共享指针
    // @param business_manager 指向业务逻辑管理器的共享指针
    // @param port 服务器监听的端口
    HTTPServer(std::shared_ptr<DatabaseManager> db_manager, 
              std::shared_ptr<BusinessManager> business_manager,
              int port = 8080);
    ~HTTPServer();

    // @brief 启动HTTP服务器
    // @return bool 启动成功返回true
    bool start();

    // @brief 停止HTTP服务器
    void stop();

    // --- Route Initialization ---

    // @brief 初始化所有API路由
    void initRoutes();

    // @brief 初始化与业务相关的路由
    void initBusinessRoutes();

    // @brief 初始化与模板相关的路由
    void initTemplateRoutes();

    // @brief 初始化与任务组相关的路由
    void initTaskGroupRoutes();

    // @brief 初始化与节点相关的路由
    void initNodeRoutes();

    // --- Route Handlers: Business Management ---

    // @brief [POST] /api/business/deploy - 部署业务
    void handleDeployBusiness(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/business/stop - 停止业务
    void handleStopBusiness(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/business/restart - 重启业务
    void handleRestartBusiness(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/businesses - 获取业务列表
    void handleGetBusinesses(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/business/details - 获取业务详情
    void handleGetBusinessDetails(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/business/component/deploy - 部署业务组件
    void handleDeployBusinessComponent(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/business/component/stop - 停止业务组件
    void handleStopBusinessComponent(const httplib::Request &req, httplib::Response &res);
    // @brief [POST] /api/business/deploy-by-template - 根据模板ID部署业务
    void handleDeployBusinessByTemplateId(const httplib::Request &req, httplib::Response &res);
    // @brief [POST] /api/business/delete - 删除业务
    void handleDeleteBusiness(const httplib::Request &req, httplib::Response &res);

    // --- Route Handlers: Template Management ---

    void handleCreateComponentTemplate(const httplib::Request& req, httplib::Response& res);
    void handleGetComponentTemplates(const httplib::Request& req, httplib::Response& res);
    void handleGetComponentTemplateDetails(const httplib::Request& req, httplib::Response& res);
    void handleUpdateComponentTemplate(const httplib::Request& req, httplib::Response& res);
    void handleDeleteComponentTemplate(const httplib::Request& req, httplib::Response& res);
    void handleCreateBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinessTemplates(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinessTemplateDetails(const httplib::Request& req, httplib::Response& res);
    void handleUpdateBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    void handleDeleteBusinessTemplate(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinessTemplateAsBusiness(const httplib::Request& req, httplib::Response& res);

    // --- Route Handlers: Task Group Management ---

    void handleTaskGroupTemplate(const httplib::Request& req, httplib::Response& res);
    void handleTaskGroupQuery(const httplib::Request& req, httplib::Response& res);
    void handleTaskGroupDeploy(const httplib::Request& req, httplib::Response& res);
    void handleTaskGroupStatus(const httplib::Request& req, httplib::Response& res);
    void handleTaskGroupStop(const httplib::Request& req, httplib::Response& res);
    void handleTaskNodeList(const httplib::Request& req, httplib::Response& res);

    // --- Route Handlers: Node Management ---

    // @brief [POST] /api/node/register - Agent节点注册
    void handleNodeRegistration(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/node/resources - Agent上报资源数据
    void handleResourceReport(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/nodes - 获取所有节点列表
    void handleGetNodes(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/node/details - 获取节点详情
    void handleGetNodeDetails(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/node/resource/history - 获取节点历史资源数据
    void handleGetNodeResourceHistory(const httplib::Request& req, httplib::Response& res);
    // @brief [GET] /api/node/resources - 获取节点当前资源数据
    void handleGetNodeResources(const httplib::Request& req, httplib::Response& res);
    // @brief [POST] /api/node/heartbeat - Agent心跳
    void handleNodeHeartbeat(const httplib::Request& req, httplib::Response& res);

    // --- Response Helpers ---

    // @brief 发送成功的JSON响应（仅消息）
    void sendSuccessResponse(httplib::Response& res, const std::string& message);
    // @brief 发送成功的JSON响应（带数据体）
    void sendSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data);
    // @brief 发送失败的JSON响应
    void sendErrorResponse(httplib::Response& res, const std::string& message);
    // @brief 发送异常信息的JSON响应
    void sendExceptionResponse(httplib::Response& res, const std::exception& e);

protected:
    httplib::Server server_;  // 底层的httplib服务器实例
    std::shared_ptr<BusinessManager> business_manager_;  // 业务逻辑管理器
    std::shared_ptr<DatabaseManager> db_manager_;    // 数据库管理器

private:
    int port_;  // 监听端口
    std::atomic<bool> running_{false};  // 服务器是否正在运行的标志
};

#endif // HTTP_SERVER_H
