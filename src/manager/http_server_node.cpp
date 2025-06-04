#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <iostream>
#include <sstream>
#include <uuid/uuid.h>

// 初始化节点管理路由
void HTTPServer::initNodeRoutes()
{
    // 资源管理

    // 注册API
    server_.Post("/api/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleNodeRegistration(req, res); });
    // 心跳API
    server_.Post("/api/heartbeat/:node_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleNodeHeartbeat(req, res); });

    // 资源监控
    server_.Post("/api/report", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceReport(req, res); });

    // 获取节点列表
    server_.Get("/api/nodes", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodes(req, res); });

    server_.Get("/api/nodes/:node_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeDetails(req, res); });

    // 获取节点资源历史
    server_.Get("/api/nodes/:node_id/resources", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeResourceHistory(req, res); });

    server_.Get("/api/nodes/:node_id/resources/:resource_type", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetNodeResources(req, res); });
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

        if (db_manager_->saveNode(json))
        {
            sendSuccessResponse(res, "node_id", node_id);
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
        
        if (db_manager_->saveResourceUsage(json))
        {
            sendSuccessResponse(res, "Resource usage saved successfully");
        }
        else
        {
            sendErrorResponse(res, "Failed to save resource usage");
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
            auto cpu_metrics = db_manager_->getCpuMetrics(node_id, 1);
            auto memory_metrics = db_manager_->getMemoryMetrics(node_id, 1);
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

// 处理获取节点资源历史
void HTTPServer::handleGetNodeResourceHistory(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string node_id = req.path_params.at("node_id");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

        auto history = db_manager_->getNodeResourceHistory(node_id, limit);
        sendSuccessResponse(res, "history", history);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetNodeResources(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto node_id = req.path_params.at("node_id");
        auto resource_type = req.path_params.at("resource_type");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

        if (req.has_param("limit"))
        {
            limit = std::stoi(req.get_param_value("limit"));
        }

        if (resource_type == "cpu")
        {
            auto cpu_metrics = db_manager_->getCpuMetrics(node_id, limit);
            sendSuccessResponse(res, "cpu_metrics", cpu_metrics);
        }
        else if (resource_type == "memory")
        {
            auto memory_metrics = db_manager_->getMemoryMetrics(node_id, limit);
            sendSuccessResponse(res, "memory_metrics", memory_metrics);
        }
        else
        {
            sendErrorResponse(res, "Invalid resource type");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理节点心跳
void HTTPServer::handleNodeHeartbeat(const httplib::Request &req, httplib::Response &res)
{
    try {
        std::string node_id = req.path_params.at("node_id");
        if (db_manager_->updateNodeLastSeen(node_id)) {
            sendSuccessResponse(res, "Heartbeat updated");
        } else {
            sendErrorResponse(res, "Failed to update heartbeat");
        }
    } catch (const std::exception &e) {
        sendExceptionResponse(res, e);
    }
}