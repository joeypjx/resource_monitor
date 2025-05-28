#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "agent_control_manager.h"
#include <iostream>
#include <sstream>
#include <uuid/uuid.h>

HTTPServer::HTTPServer(std::shared_ptr<DatabaseManager> db_manager,
                       std::shared_ptr<BusinessManager> business_manager,
                       std::shared_ptr<AgentControlManager> agent_control_manager,
                       int port)
    : db_manager_(db_manager), business_manager_(business_manager), agent_control_manager_(agent_control_manager), port_(port), running_(false)
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

    // 初始化模板管理路由
    initTemplateRoutes();

    // 初始化业务管理路由
    initBusinessRoutes();

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

void HTTPServer::initRoutes()
{
    // 注册API
    server_.Post("/api/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleBoardRegistration(req, res); });
    // 上报API
    server_.Post("/api/report", [this](const httplib::Request &req, httplib::Response &res)
                 { handleResourceReport(req, res); });
    // 心跳API
    server_.Post("/api/heartbeat/:board_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleBoardHeartbeat(req, res); });
    // 控制API
    server_.Post("/api/boards/:board_id/control", [this](const httplib::Request &req, httplib::Response &res)
                 { handleBoardControl(req, res); });         

    // 板卡管理API
    server_.Get("/api/boards", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoards(req, res); });

    server_.Get("/api/boards/:board_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardDetails(req, res); });

    server_.Get("/api/boards/:board_id/resources", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardResourceHistory(req, res); });

    server_.Get("/api/boards/:board_id/resources/:resource_type", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardResources(req, res); });
    
    server_.Get("/api/cluster/metrics", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetClusterMetrics(req, res); });

    server_.Get("/api/cluster/metrics/history", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetClusterMetricsHistory(req, res); });

    // 机箱管理API
    server_.Post("/api/chassis", [this](const httplib::Request &req, httplib::Response &res)
                 { handleCreateChassis(req, res); });

    server_.Get("/api/chassis", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassis(req, res); });

    server_.Get("/api/chassis/:chassis_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassisDetails(req, res); });

    server_.Put("/api/chassis/:chassis_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleUpdateChassis(req, res); });

    server_.Delete("/api/chassis/:chassis_id", [this](const httplib::Request &req, httplib::Response &res)
                   { handleDeleteChassis(req, res); });

    server_.Get("/api/chassis/:chassis_id/boards", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassisBoards(req, res); });

    // CPU和GPU管理API
    server_.Get("/api/boards/:board_id/cpus", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardCpus(req, res); });

    server_.Get("/api/boards/:board_id/gpus", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetBoardGpus(req, res); });

    server_.Get("/api/cpus", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAllCpus(req, res); });

    server_.Get("/api/gpus", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetAllGpus(req, res); });
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

// 机箱管理处理函数

void HTTPServer::handleCreateChassis(const httplib::Request& req, httplib::Response& res) {
    try {
        auto chassis_info = nlohmann::json::parse(req.body);
        
        if (db_manager_->saveChassis(chassis_info)) {
            sendSuccessResponse(res, "Chassis created successfully");
        } else {
            sendErrorResponse(res, "Failed to create chassis");
        }
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetChassis(const httplib::Request& req, httplib::Response& res) {
    try {
        auto chassis_list = db_manager_->getChassis();
        sendSuccessResponse(res, "chassis_list", chassis_list);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetChassisDetails(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string chassis_id = req.path_params.at("chassis_id");
        auto chassis = db_manager_->getChassisById(chassis_id);
        
        if (chassis.empty()) {
            sendErrorResponse(res, "Chassis not found");
        } else {
            sendSuccessResponse(res, "chassis", chassis);
        }
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleUpdateChassis(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string chassis_id = req.path_params.at("chassis_id");
        auto chassis_info = nlohmann::json::parse(req.body);
        
        if (db_manager_->updateChassis(chassis_id, chassis_info)) {
            sendSuccessResponse(res, "Chassis updated successfully");
        } else {
            sendErrorResponse(res, "Failed to update chassis");
        }
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleDeleteChassis(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string chassis_id = req.path_params.at("chassis_id");
        
        if (db_manager_->deleteChassis(chassis_id)) {
            sendSuccessResponse(res, "Chassis deleted successfully");
        } else {
            sendErrorResponse(res, "Failed to delete chassis or chassis has associated boards");
        }
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetChassisBoards(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string chassis_id = req.path_params.at("chassis_id");
        auto boards = db_manager_->getBoardsByChassis(chassis_id);
        
        sendSuccessResponse(res, "boards", boards);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

// CPU和GPU管理处理函数

void HTTPServer::handleGetBoardCpus(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string board_id = req.path_params.at("board_id");
        auto cpus = db_manager_->getBoardCpus(board_id);
        
        sendSuccessResponse(res, "cpus", cpus);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetBoardGpus(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string board_id = req.path_params.at("board_id");
        auto gpus = db_manager_->getBoardGpus(board_id);
        
        sendSuccessResponse(res, "gpus", gpus);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetAllCpus(const httplib::Request& req, httplib::Response& res) {
    try {
        auto cpus = db_manager_->getAllCpus();
        
        sendSuccessResponse(res, "cpus", cpus);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleGetAllGpus(const httplib::Request& req, httplib::Response& res) {
    try {
        auto gpus = db_manager_->getAllGpus();
        
        sendSuccessResponse(res, "gpus", gpus);
    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
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