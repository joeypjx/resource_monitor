#include "http_server.h"
#include "business_manager.h"
#include <iostream>
#include <nlohmann/json.hpp>

// 初始化业务管理路由
void HTTPServer::initBusinessRoutes()
{
    // 业务管理API
    server_.Post("/api/businesses/template/:business_template_id", [this](const httplib::Request &req, httplib::Response &res)
                 { handleDeployBusinessByTemplateId(req, res); });

    server_.Post("/api/businesses", [this](const httplib::Request &req, httplib::Response &res)
                 { handleDeployBusiness(req, res); });

    server_.Post("/api/businesses/:business_id/stop", [this](const httplib::Request &req, httplib::Response &res)
                 { handleStopBusiness(req, res); });

    server_.Post("/api/businesses/:business_id/restart", [this](const httplib::Request &req, httplib::Response &res)
                 { handleRestartBusiness(req, res); });

    server_.Delete("/api/businesses/:business_id", [this](const httplib::Request &req, httplib::Response &res)
                   { handleDeleteBusiness(req, res); });

    // 处理业务列表和详情
    server_.Get("/api/businesses", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinesses(req, res); });

    server_.Get("/api/businesses/:business_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessDetails(req, res); });

    // 处理业务组件部署和停止
    server_.Post("/api/businesses/:business_id/components/:component_id/deploy", [this](const httplib::Request &req, httplib::Response &res)
                 { handleDeployBusinessComponent(req, res); });

    server_.Post("/api/businesses/:business_id/components/:component_id/stop", [this](const httplib::Request &req, httplib::Response &res)
                 { handleStopBusinessComponent(req, res); });
}

// 处理业务部署（通过模板ID）
void HTTPServer::handleDeployBusinessByTemplateId(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_template_id = req.path_params.at("business_template_id");
        auto result = business_manager_->deployBusinessByTemplateId(business_template_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
    }
}

// 处理业务部署
void HTTPServer::handleDeployBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->deployBusiness(json);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
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

//
void HTTPServer::handleDeleteBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->deleteBusiness(business_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(e.what(), "application/json");
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

// 新增：处理部署某个业务的某个组件
void HTTPServer::handleDeployBusinessComponent(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        std::string component_id = req.path_params.at("component_id");

        auto result = business_manager_->deployComponent(business_id, component_id);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump().c_str(), "application/json");
    }
}

// 新增：处理停止某个业务的某个组件
void HTTPServer::handleStopBusinessComponent(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        std::string component_id = req.path_params.at("component_id");
        auto result = business_manager_->stopComponent(business_id, component_id, false);
        res.set_content(result.dump().c_str(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump().c_str(), "application/json");
    }
}