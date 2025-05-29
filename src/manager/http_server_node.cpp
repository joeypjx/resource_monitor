#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "agent_control_manager.h"
#include <iostream>
#include <sstream>
#include <uuid/uuid.h>

// 初始化节点和板卡管理路由
void HTTPServer::initNodeRoutes()
{
    // 资源管理

    // 注册API
    server_.Post("/api/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleBoardRegistration(req, res); });
    // 心跳API
    server_.Post("/api/heartbeat/:board_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleBoardHeartbeat(req, res); });
    // 控制API
    server_.Post("/api/boards/:board_id/control", [this](const httplib::Request &req, httplib::Response &res)
                 { handleBoardControl(req, res); });         

    server_.Get("/api/boards", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoards(req, res); });

    server_.Get("/api/boards/:board_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardDetails(req, res); });

    // 资源监控
    server_.Post("/api/report", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceReport(req, res); });

    server_.Get("/api/boards/:board_id/resources", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardResourceHistory(req, res); });

    server_.Get("/api/boards/:board_id/resources/:resource_type", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardResources(req, res); });
    
    server_.Get("/api/cluster/metrics", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetClusterMetrics(req, res); });

    server_.Get("/api/cluster/metrics/history", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetClusterMetricsHistory(req, res); });
}

// 处理板卡注册
void HTTPServer::handleBoardRegistration(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto json = nlohmann::json::parse(req.body);
        std::string board_id;
        if (!json.contains("board_id") || json["board_id"].get<std::string>().empty()) {
            // 生成新的board_id
            uuid_t uuid;
            char uuid_str[37];
            uuid_generate(uuid);
            uuid_unparse_lower(uuid, uuid_str);
            board_id = std::string("board-") + uuid_str;
            json["board_id"] = board_id;
        } else {
            board_id = json["board_id"].get<std::string>();
        }

        if (db_manager_->saveBoard(json))
        {
            sendSuccessResponse(res, "board_id", board_id);
        }
        else
        {
            sendErrorResponse(res, "Failed to register board");
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

// 处理获取板卡列表
void HTTPServer::handleGetBoards(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto boards = db_manager_->getBoards();
        sendSuccessResponse(res, "boards", boards);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取板卡详情
void HTTPServer::handleGetBoardDetails(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string board_id = req.path_params.at("board_id");
        auto board = db_manager_->getBoard(board_id);

        if (!board.empty())
        {
            sendSuccessResponse(res, "board", board);
        }
        else
        {
            sendErrorResponse(res, "Board not found");
        }
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

// 处理获取板卡资源历史
void HTTPServer::handleGetBoardResourceHistory(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string board_id = req.path_params.at("board_id");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

        auto history = db_manager_->getNodeResourceHistory(board_id, limit);
        sendSuccessResponse(res, "history", history);
    }
    catch (const std::exception &e)
    {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetBoardResources(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto board_id = req.path_params.at("board_id");
        auto resource_type = req.path_params.at("resource_type");
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;

        if (req.has_param("limit"))
        {
            limit = std::stoi(req.get_param_value("limit"));
        }

        if (resource_type == "cpu")
        {
            auto cpu_metrics = db_manager_->getCpuMetrics(board_id, limit);
            sendSuccessResponse(res, "cpu_metrics", cpu_metrics);
        }
        else if (resource_type == "memory")
        {
            auto memory_metrics = db_manager_->getMemoryMetrics(board_id, limit);
            sendSuccessResponse(res, "memory_metrics", memory_metrics);
        }
        else if (resource_type == "disk")
        {
            auto disk_metrics = db_manager_->getDiskMetrics(board_id, limit);
            sendSuccessResponse(res, "disk_metrics", disk_metrics);
        }
        else if (resource_type == "network")
        {
            auto network_metrics = db_manager_->getNetworkMetrics(board_id, limit);
            sendSuccessResponse(res, "network_metrics", network_metrics);
        }
        else if (resource_type == "docker")
        {
            auto docker_metrics = db_manager_->getDockerMetrics(board_id, limit);
            sendSuccessResponse(res, "docker_metrics", docker_metrics);
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

void HTTPServer::handleGetClusterMetrics(const httplib::Request& req, httplib::Response& res)
{
    try {
        auto metrics = db_manager_->getClusterMetrics();
        sendSuccessResponse(res, "metrics", metrics);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetClusterMetricsHistory(const httplib::Request& req, httplib::Response& res)
{
    try {
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 100;
        auto history = db_manager_->getClusterMetricsHistory(limit);
        sendSuccessResponse(res, "history", history);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

// 处理Board心跳
void HTTPServer::handleBoardHeartbeat(const httplib::Request &req, httplib::Response &res)
{
    try {
        std::string board_id = req.path_params.at("board_id");
        if (db_manager_->updateBoardLastSeen(board_id)) {
            sendSuccessResponse(res, "Heartbeat updated");
        } else {
            sendErrorResponse(res, "Failed to update heartbeat");
        }
    } catch (const std::exception &e) {
        sendExceptionResponse(res, e);
    }
}

// 处理Board控制
void HTTPServer::handleBoardControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string board_id = req.path_params.at("board_id");
        auto request_json = nlohmann::json::parse(req.body);
        if (!agent_control_manager_) {
            sendErrorResponse(res, "AgentControlManager not initialized");
            return;
        }
        auto result = agent_control_manager_->controlAgent(board_id, request_json);
        sendSuccessResponse(res, "result", result);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
} 