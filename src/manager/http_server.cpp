#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <iostream>

HTTPServer::HTTPServer(std::shared_ptr<DatabaseManager> db_manager,
                       std::shared_ptr<BusinessManager> business_manager,
                       int port)
    : db_manager_(db_manager), business_manager_(business_manager), port_(port), running_(false)
{
    
}

HTTPServer::~HTTPServer()
{
    if (running_)
    {
        stop();
    }
}

bool HTTPServer::start()
{
    std::cout << "Starting HTTP server on port " << port_ << std::endl;

    // 初始化节点管理路由
    initNodeRoutes();

    // 初始化模板管理路由
    initTemplateRoutes();

    // 初始化业务管理路由
    initBusinessRoutes();

    // web ui
    server_.set_mount_point("/", "./web");

    // 启动服务器
    running_ = true;
    server_.set_default_headers({{"Access-Control-Allow-Origin", "*"}, {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"}, {"Access-Control-Allow-Headers", "Content-Type"}});
    server_.listen("0.0.0.0", port_);

    return true;
}

void HTTPServer::stop()
{
    if (running_)
    {
        std::cout << "Stopping HTTP server" << std::endl;
        server_.stop();
        running_ = false;
    }
}

// 响应辅助方法
void HTTPServer::sendSuccessResponse(httplib::Response& res, const std::string& message) {
    res.set_content(nlohmann::json({{"status", "success"}, {"message", message}}).dump(), "application/json");
}

void HTTPServer::sendSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data) {
    res.set_content(nlohmann::json({{"status", "success"}, {key, data}}).dump(), "application/json");
}

void HTTPServer::sendErrorResponse(httplib::Response& res, const std::string& message) {
    res.set_content(nlohmann::json({{"status", "error"}, {"message", message}}).dump(), "application/json");
}

void HTTPServer::sendExceptionResponse(httplib::Response& res, const std::exception& e) {
    sendErrorResponse(res, e.what());
} 