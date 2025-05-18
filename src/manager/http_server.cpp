#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <iostream>
#include <sstream>

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

    // 初始化路由
    initRoutes();

    // 启动服务器
    running_ = true;
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

void HTTPServer::initRoutes()
{
    // 节点管理API
    server_.Post("/api/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleNodeRegistration(req, res); });

    server_.Post("/api/report", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceReport(req, res); });

    server_.Get("/api/agents", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodes(req, res); });

    server_.Get("/api/agents/:node_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeDetails(req, res); });

    server_.Get("/api/agents/:agent_id/resources/:resource_type", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAgentResources(req, res); });

    server_.Get("/api/agents/:agent_id/resources", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetNodeResourceHistory(req, res);
    });

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

    server_.Get("/api/templates/businesses/:template_id/as-business", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBusinessTemplateAsBusiness(req, res); });
}

// 处理节点注册
void HTTPServer::handleNodeRegistration(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);

        if (db_manager_->saveAgent(json))
        {
            res.set_content("{\"status\":\"success\",\"message\":\"Node registered successfully\"}", "application/json");
        }
        else
        {
            res.set_content("{\"status\":\"error\",\"message\":\"Failed to register node\"}", "application/json");
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理资源上报
void HTTPServer::handleResourceReport(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);

        if (db_manager_->saveResourceUsage(json))
        {
            res.set_content("{\"status\":\"success\",\"message\":\"Resource usage saved successfully\"}", "application/json");
        }
        else
        {
            res.set_content("{\"status\":\"error\",\"message\":\"Failed to save resource usage\"}", "application/json");
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取节点列表
void HTTPServer::handleGetNodes(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto nodes = db_manager_->getAgents();
        res.set_content(nodes.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取节点详情
void HTTPServer::handleGetNodeDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string node_id = req.path_params.at("node_id");
        auto node = db_manager_->getNode(node_id);

        if (!node.empty())
        {
            res.set_content(node.dump(), "application/json");
        }
        else
        {
            res.set_content("{\"status\":\"error\",\"message\":\"Node not found\"}", "application/json");
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取节点资源
void HTTPServer::handleGetNodeResourceHistory(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string node_id = req.path_params.at("agent_id");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

        auto history = db_manager_->getNodeResourceHistory(node_id, limit);
        res.set_content(history.dump(), "application/json");
    } catch (const std::exception& e) {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

void HTTPServer::handleGetAgentResources(const httplib::Request &req, httplib::Response &res)
{

    auto agent_id = req.path_params.at("agent_id");
    auto resource_type = req.path_params.at("resource_type");
    int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

    try
    {
        if (req.has_param("limit"))
        {
            limit = std::stoi(req.get_param_value("limit"));
        }

        if (resource_type == "cpu")
        {
            auto cpu_metrics = db_manager_->getCpuMetrics(agent_id, limit);
            res.set_content(cpu_metrics.dump(), "application/json");
        }
        else if (resource_type == "memory")
        {
            auto memory_metrics = db_manager_->getMemoryMetrics(agent_id, limit);
            res.set_content(memory_metrics.dump(), "application/json");
        }
        else if (resource_type == "disk")
        {
            auto disk_metrics = db_manager_->getDiskMetrics(agent_id, limit);
            res.set_content(disk_metrics.dump(), "application/json");
        }
        else if (resource_type == "network")
        {
            auto network_metrics = db_manager_->getNetworkMetrics(agent_id, limit);
            res.set_content(network_metrics.dump(), "application/json");
        }
        else if (resource_type == "docker")
        {
            auto docker_metrics = db_manager_->getDockerMetrics(agent_id, limit);
            res.set_content(docker_metrics.dump(), "application/json");
        }
        else
        {
            res.set_content("{\"status\":\"error\",\"message\":\"Invalid resource type\"}", "application/json");
        }
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理业务部署
void HTTPServer::handleDeployBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->deployBusiness(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理业务停止
void HTTPServer::handleStopBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->stopBusiness(business_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理业务重启
void HTTPServer::handleRestartBusiness(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->restartBusiness(business_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取业务列表
void HTTPServer::handleGetBusinesses(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto result = business_manager_->getBusinesses();
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取业务详情
void HTTPServer::handleGetBusinessDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->getBusinessDetails(business_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理获取业务组件
void HTTPServer::handleGetBusinessComponents(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string business_id = req.path_params.at("business_id");
        auto result = business_manager_->getBusinessComponents(business_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理组件状态上报
void HTTPServer::handleComponentStatusReport(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = business_manager_->handleComponentStatusReport(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理创建组件模板
void HTTPServer::handleCreateComponentTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = db_manager_->saveComponentTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        auto result = db_manager_->saveComponentTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理删除组件模板
void HTTPServer::handleDeleteComponentTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto result = db_manager_->deleteComponentTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理创建业务模板
void HTTPServer::handleCreateBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        auto result = db_manager_->saveBusinessTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理更新业务模板
void HTTPServer::handleUpdateBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.get_param_value("template_id");
        auto json = nlohmann::json::parse(req.body);
        json["business_template_id"] = template_id;
        auto result = db_manager_->saveBusinessTemplate(json);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

// 处理删除业务模板
void HTTPServer::handleDeleteBusinessTemplate(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string template_id = req.path_params.at("template_id");
        auto result = db_manager_->deleteBusinessTemplate(template_id);
        res.set_content(result.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content("{\"status\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}", "application/json");
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
            res.set_content(result.dump(), "application/json");
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
            if (comp.contains("component_id"))
                c["component_id"] = comp["component_id"];
            if (comp.contains("component_name"))
                c["component_name"] = comp["component_name"];
            if (comp.contains("template_details"))
            {
                const auto &details = comp["template_details"];
                c["type"] = details["type"];
                // 合并config内容
                for (auto it = details["config"].begin(); it != details["config"].end(); ++it)
                {
                    c[it.key()] = it.value();
                }
            }
            business_json["components"].push_back(c);
        }
        res.set_content((nlohmann::json{{"status", "success"}, {"business", business_json}}).dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        res.set_content((nlohmann::json{{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
    }
}
