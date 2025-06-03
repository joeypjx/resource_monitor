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
    // 构造与析构
    HTTPServer(std::shared_ptr<DatabaseManager> db_manager, 
              std::shared_ptr<BusinessManager> business_manager,
              int port = 8080);
    ~HTTPServer();

    // 启动与停止
    bool start();
    void stop();

    // 路由初始化
    void initRoutes();
    void initBusinessRoutes();
    void initTemplateRoutes();

    // 路由初始化
    void initNodeRoutes();

    // 业务管理相关
    void handleDeployBusiness(const httplib::Request& req, httplib::Response& res);
    void handleStopBusiness(const httplib::Request& req, httplib::Response& res);
    void handleRestartBusiness(const httplib::Request& req, httplib::Response& res);
    void handleUpdateBusiness(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinesses(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinessDetails(const httplib::Request& req, httplib::Response& res);
    void handleGetBusinessComponents(const httplib::Request& req, httplib::Response& res);
    void handleComponentStatusReport(const httplib::Request& req, httplib::Response& res);

    // 模板管理相关
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

    // 板卡管理相关
    void handleBoardRegistration(const httplib::Request& req, httplib::Response& res);
    void handleResourceReport(const httplib::Request& req, httplib::Response& res);
    void handleGetBoards(const httplib::Request& req, httplib::Response& res);
    void handleGetBoardDetails(const httplib::Request& req, httplib::Response& res);
    void handleGetBoardResourceHistory(const httplib::Request& req, httplib::Response& res);
    void handleGetBoardResources(const httplib::Request& req, httplib::Response& res);
    void handleBoardHeartbeat(const httplib::Request& req, httplib::Response& res);

    // 响应辅助方法
    void sendSuccessResponse(httplib::Response& res, const std::string& message);
    void sendSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data);
    void sendErrorResponse(httplib::Response& res, const std::string& message);
    void sendExceptionResponse(httplib::Response& res, const std::exception& e);

protected:
    httplib::Server server_;  // HTTP服务器
    std::shared_ptr<BusinessManager> business_manager_;  // 业务管理器
    std::shared_ptr<DatabaseManager> db_manager_;    // 数据库管理器

private:
    int port_;  // 监听端口
    bool running_;  // 是否正在运行
};

#endif // HTTP_SERVER_H
