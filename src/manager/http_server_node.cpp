#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "utils/logger.h"
#include <iostream>
#include <sstream>
#include <uuid/uuid.h>

// 初始化节点管理路由
void HTTPServer::initNodeRoutes()
{
    // 注册API
    server_.Post("/api/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleNodeRegistration(req, res); });

    // 资源监控
    server_.Post("/api/report", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceReport(req, res); });

    // 获取节点列表
    server_.Get("/api/nodes", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodes(req, res); });

    server_.Get("/api/nodes/:node_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeDetails(req, res); });
}

// 处理节点注册
void HTTPServer::handleNodeRegistration(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        std::string node_id;
        if (!json.contains("node_id") || json["node_id"].get<std::string>().empty()) {
            // 生成新的board_id
            uuid_t uuid;
            char uuid_str[37];
            uuid_generate(uuid);
            uuid_unparse_lower(uuid, uuid_str);
            node_id = std::string("node-") + uuid_str;
            json["node_id"] = node_id;
        } else {
            node_id = json["node_id"].get<std::string>();
        }

        LOG_INFO("Node registered: {}", json.dump(4));

        if (db_manager_->saveNode(json))
        {
            // 查询该节点下所有已分配组件
            nlohmann::json components = db_manager_->getComponentsByNodeId(node_id);
            nlohmann::json resp = {
                {"status", "success"},
                {"node_id", node_id},
                {"components", components}
            };
            res.set_content(resp.dump(), "application/json");
        }
        else
        {
            sendErrorResponse(res, "Failed to register node");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理资源上报
void HTTPServer::handleResourceReport(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        
        // 提前检查并更新节点状态
        if (json.contains("node_id")) {
            std::string node_id = json["node_id"].get<std::string>();
            db_manager_->updateNodeLastSeen(node_id);
        } else {
            sendErrorResponse(res, "Missing node_id in resource report");
            return;
        }

        if (db_manager_->saveResourceUsage(json))
        {
            sendSuccessResponse(res, "Resource usage saved successfully");
        }
        else
        {
            sendErrorResponse(res, "Failed to save resource usage");
        }

        // 保存组件状态
        if (json.contains("components")) {
            for (const auto& component : json["components"]) {
                db_manager_->updateComponentStatus(component);
            }
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取节点列表
void HTTPServer::handleGetNodes(const httplib::Request &req, httplib::Response &res)
{
    (void)req;
    try
    {
        auto nodes = db_manager_->getNodes();
        sendSuccessResponse(res, "nodes", nodes);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
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
            // 获取最新CPU和内存资源状态
            auto cpu_metrics = db_manager_->getCpuMetrics(node_id);
            auto memory_metrics = db_manager_->getMemoryMetrics(node_id);
            if (!cpu_metrics.empty()) {
                node["latest_cpu"] = cpu_metrics[0];
            }
            if (!memory_metrics.empty()) {
                node["latest_memory"] = memory_metrics[0];
            }
            sendSuccessResponse(res, "node", node);
        }
        else
        {
            sendErrorResponse(res, "Node not found");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}