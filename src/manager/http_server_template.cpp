#include "http_server.h"
#include "database_manager.h"
#include "utils/logger.h"
#include <iostream>
#include <nlohmann/json.hpp>

// 初始化模板管理路由
void HTTPServer::initTemplateRoutes()
{
    // 组件模板API
    server_.Post("/api/templates/components", [this](const httplib::Request &req, httplib::Response &res)
                 { handleCreateComponentTemplate(req, res); });

    server_.Get("/api/templates/components", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetComponentTemplates(req, res); });

    server_.Get("/api/templates/components/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetComponentTemplateDetails(req, res); });

    server_.Put("/api/templates/components/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleUpdateComponentTemplate(req, res); });

    server_.Delete("/api/templates/components/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                   { handleDeleteComponentTemplate(req, res); });

    // 业务模板API
    server_.Post("/api/templates/businesses", [this](const httplib::Request &req, httplib::Response &res)
                 { handleCreateBusinessTemplate(req, res); });

    server_.Get("/api/templates/businesses", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessTemplates(req, res); });

    server_.Get("/api/templates/businesses/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessTemplateDetails(req, res); });

    server_.Put("/api/templates/businesses/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleUpdateBusinessTemplate(req, res); });

    server_.Delete("/api/templates/businesses/:template_id", [this](const httplib::Request &req, httplib::Response &res)
                   { handleDeleteBusinessTemplate(req, res); });

    // 业务模板转换为业务API
    server_.Get("/api/templates/businesses/:template_id/as-business", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessTemplateAsBusiness(req, res); });
}

// 处理创建组件模板
void HTTPServer::handleCreateComponentTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        LOG_INFO("Creating component template: {}", json.dump(4));
        
        auto result = db_manager_->saveComponentTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理获取组件模板列表
void HTTPServer::handleGetComponentTemplates(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto result = db_manager_->getComponentTemplates();
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理获取组件模板详情
void HTTPServer::handleGetComponentTemplateDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto result = db_manager_->getComponentTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理更新组件模板
void HTTPServer::handleUpdateComponentTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto json = nlohmann::json::parse(req.body);
        json["component_template_id"] = template_id;
        LOG_INFO("Updating component template: {}", json.dump(4));
        auto result = db_manager_->saveComponentTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理删除组件模板
void HTTPServer::handleDeleteComponentTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        LOG_INFO("Deleting component template: {}", template_id);

        auto result = db_manager_->deleteComponentTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理创建业务模板
void HTTPServer::handleCreateBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        LOG_INFO("Creating business template: {}", json.dump(4));
        auto result = db_manager_->saveBusinessTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理获取业务模板列表
void HTTPServer::handleGetBusinessTemplates(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto result = db_manager_->getBusinessTemplates();
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理获取业务模板详情
void HTTPServer::handleGetBusinessTemplateDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto result = db_manager_->getBusinessTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理更新业务模板
void HTTPServer::handleUpdateBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto json = nlohmann::json::parse(req.body);
        json["business_template_id"] = template_id;
        LOG_INFO("Updating business template: {}", json.dump(4));
        auto result = db_manager_->saveBusinessTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 处理删除业务模板
void HTTPServer::handleDeleteBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        LOG_INFO("Deleting business template: {}", template_id);
        auto result = db_manager_->deleteBusinessTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}

// 新增：获取业务模板并转换为部署业务格式
void HTTPServer::handleGetBusinessTemplateAsBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto result = db_manager_->getBusinessTemplate(template_id);
        if (result["status"] != "success")
        {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", "Failed to get business template"}}).dump(), "application/json");
            return;
        }
        auto tpl = result["template"];
        nlohmann::json business_json;
        business_json["business_name"] = tpl["template_name"];
        business_json["components"] = nlohmann::json::array();
        for (const auto &comp : tpl["components"])
        {
            nlohmann::json c;
            // 兼容模板中有component_id/component_name
            if (comp.contains("template_details"))
            {
                const auto &details = comp["template_details"];
                c["type"] = details["type"];
                c["component_id"] = details["component_template_id"];
                c["component_name"] = details["template_name"];
                // 合并config内容
                for (auto it = details["config"].begin(); it != details["config"].end(); ++it)
                {
                    c[it.key()] = it.value();
                }
            }
            business_json["components"].push_back(c);
        }
        res.set_content(business_json.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
} 