#include "http_server.h"
#include "business_manager.h"
#include <iostream>
#include <nlohmann/json.hpp>

// 初始化业务管理路由
void HTTPServer::initBusinessRoutes()
{
    // 业务管理API
    server_.Post("/api/businesses", [this](const httplib::Request &req, httplib::Response &res)
                 { handleDeployBusiness(req, res); });

    server_.Post("/api/businesses/:business_id/stop", [this](const httplib::Request &req, httplib::Response &res)
                 { handleStopBusiness(req, res); });

    server_.Post("/api/businesses/:business_id/restart", [this](const httplib::Request &req, httplib::Response &res)
                 { handleRestartBusiness(req, res); });

    server_.Put("/api/businesses/:business_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleUpdateBusiness(req, res); });

    server_.Get("/api/businesses", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinesses(req, res); });

    server_.Get("/api/businesses/:business_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessDetails(req, res); });

    server_.Get("/api/businesses/:business_id/components", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessComponents(req, res); });

    server_.Post("/api/report/component", [this](const httplib::Request &req, httplib::Response &res)
                 { handleComponentStatusReport(req, res); });

    server_.Post("/api/businesses/template/:business_template_id", [this](const httplib::Request &req, httplib::Response &res)
                 { 
                    std::string business_template_id = req.path_params.at("business_template_id");
                    auto result = business_manager_->deployBusinessByTemplateId(business_template_id);
                    res.set_content(result.dump().c_str(), "application/json");
                  });

    server_.Delete("/api/businesses/:business_id", [this](const httplib::Request &req, httplib::Response &res)
    {
        try {
            std::string business_id = req.path_params.at("business_id");
            auto result = business_manager_->deleteBusiness(business_id);
            sendSuccessResponse(res, "result", result);
        } catch (const std::exception &e) {
            sendExceptionResponse(res, e);
        }
    });
}

// 处理业务部署
void HTTPServer::handleDeployBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->deployBusiness(json);
        std::cout << "handleDeployBusiness result: " << result.dump() << std::endl;
        sendSuccessResponse(res, "result", result);
    }
    catch (const std::exception &e)
    {
        std::cout << "handleDeployBusiness error: " << e.what() << std::endl;
        sendExceptionResponse(res, e);
    }
}

// 处理业务停止
void HTTPServer::handleStopBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->stopBusiness(business_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
    }
}

// 处理业务重启
void HTTPServer::handleRestartBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->restartBusiness(business_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
    }
}

// 处理业务更新
void HTTPServer::handleUpdateBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->updateBusiness(business_id, json);
        sendSuccessResponse(res, "result", result);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取业务列表
void HTTPServer::handleGetBusinesses(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto result = business_manager_->getBusinesses();
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
    }
}

// 处理获取业务详情
void HTTPServer::handleGetBusinessDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->getBusinessDetails(business_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
    }
}

// 处理获取业务组件
void HTTPServer::handleGetBusinessComponents(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->getBusinessComponents(business_id);
        sendSuccessResponse(res, "result", result);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理组件状态上报
void HTTPServer::handleComponentStatusReport(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->handleComponentStatusReport(json);
        sendSuccessResponse(res, "result", result);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
} 