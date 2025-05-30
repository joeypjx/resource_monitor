#include "http_server.h"
#include "database_manager.h"
#include <iostream>
#include <nlohmann/json.hpp>

// Chassis专用响应方法实现
void HTTPServer::sendChassisSuccessResponse(httplib::Response& res, const std::string& message) {
    nlohmann::json response = {
        {"apiVersion", 1},
        {"status", "success"},
        {"data", {
            {"message", message}
        }}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendChassisSuccessResponse(httplib::Response& res, const std::string& key, const nlohmann::json& data) {
    nlohmann::json response = {
        {"apiVersion", 1},
        {"status", "success"},
        {"data", {
            {key, data}
        }}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendChassisErrorResponse(httplib::Response& res, const std::string& message) {
    nlohmann::json response = {
        {"apiVersion", 1},
        {"status", "error"},
        {"data", {
            {"message", message}
        }}
    };
    res.set_content(response.dump(), "application/json");
}

void HTTPServer::sendChassisExceptionResponse(httplib::Response& res, const std::exception& e) {
    sendChassisErrorResponse(res, e.what());
}

// 初始化chassis和slot管理路由
void HTTPServer::initChassisRoutes()
{
    // Chassis管理API
    server_.Get("/api/chassis", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassisList(req, res); });

    server_.Get("/api/chassis/detailed", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassisDetailedList(req, res); });

    server_.Get("/api/chassis/:chassis_id", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetChassisInfo(req, res); });

    server_.Post("/api/chassis", [this](const httplib::Request &req, httplib::Response &res)
                 { handleCreateOrUpdateChassis(req, res); });

    // Slot管理API
    server_.Get("/api/slots", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetSlots(req, res); });

    server_.Get("/api/chassis/:chassis_id/slots/:slot_index", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetSlotInfo(req, res); });

    server_.Post("/api/chassis/:chassis_id/slots/:slot_index", [this](const httplib::Request &req, httplib::Response &res)
                 { handleUpdateSlot(req, res); });

    server_.Put("/api/chassis/:chassis_id/slots/:slot_index/status", [this](const httplib::Request &req, httplib::Response &res)
                { handleUpdateSlotStatus(req, res); });

    // GET /api/slots/with-metrics - 获取所有slots及其最新metrics
    server_.Get("/api/slots/with-metrics", [this](const httplib::Request &req, httplib::Response &res)
                { handleGetSlotsWithMetrics(req, res); });

    // POST /register - 注册/更新slot信息（从body获取chassis_id和slot_index）
    server_.Post("/register", [this](const httplib::Request &req, httplib::Response &res)
                 { handleRegisterSlot(req, res); });

    // POST /resource - 更新slot的metrics数据
    server_.Post("/resource", [this](const httplib::Request &req, httplib::Response &res)
                 { handleUpdateSlotMetrics(req, res); });
}

// 处理获取chassis列表
void HTTPServer::handleGetChassisList(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto chassis_list = db_manager_->getChassisList();
        sendChassisSuccessResponse(res, "chassis_list", chassis_list);
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理获取chassis信息（包含所有slots）
void HTTPServer::handleGetChassisInfo(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string chassis_id = req.path_params.at("chassis_id");
        auto chassis_info = db_manager_->getChassisInfo(chassis_id);
        
        if (!chassis_info.empty())
        {
            sendChassisSuccessResponse(res, "chassis", chassis_info);
        }
        else
        {
            sendChassisErrorResponse(res, "Chassis not found");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理创建或更新chassis
void HTTPServer::handleCreateOrUpdateChassis(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto request_json = nlohmann::json::parse(req.body);
        
        // 检查API版本和data字段
        if (!request_json.contains("data"))
        {
            sendChassisErrorResponse(res, "Missing data field in request");
            return;
        }
        
        auto json = request_json["data"];
        
        if (!json.contains("chassis_id"))
        {
            sendChassisErrorResponse(res, "chassis_id is required");
            return;
        }
        
        if (db_manager_->saveChassis(json))
        {
            sendChassisSuccessResponse(res, "Chassis saved successfully");
        }
        else
        {
            sendChassisErrorResponse(res, "Failed to save chassis");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理获取所有slots列表
void HTTPServer::handleGetSlots(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto slots = db_manager_->getSlots();
        sendChassisSuccessResponse(res, "slots", slots);
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理获取特定slot信息
void HTTPServer::handleGetSlotInfo(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string chassis_id = req.path_params.at("chassis_id");
        int slot_index = std::stoi(req.path_params.at("slot_index"));
        
        auto slot_info = db_manager_->getSlot(chassis_id, slot_index);
        
        if (!slot_info.empty())
        {
            sendChassisSuccessResponse(res, "slot", slot_info);
        }
        else
        {
            sendChassisErrorResponse(res, "Slot not found");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理更新slot信息
void HTTPServer::handleUpdateSlot(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string chassis_id = req.path_params.at("chassis_id");
        int slot_index = std::stoi(req.path_params.at("slot_index"));
        
        auto request_json = nlohmann::json::parse(req.body);
        
        // 检查data字段
        if (!request_json.contains("data"))
        {
            sendChassisErrorResponse(res, "Missing data field in request");
            return;
        }
        
        auto json = request_json["data"];
        json["chassis_id"] = chassis_id;
        json["slot_index"] = slot_index;
        
        if (db_manager_->saveSlot(json))
        {
            sendChassisSuccessResponse(res, "Slot updated successfully");
        }
        else
        {
            sendChassisErrorResponse(res, "Failed to update slot");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理更新slot状态
void HTTPServer::handleUpdateSlotStatus(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        std::string chassis_id = req.path_params.at("chassis_id");
        int slot_index = std::stoi(req.path_params.at("slot_index"));
        
        auto request_json = nlohmann::json::parse(req.body);
        
        // 检查data字段
        if (!request_json.contains("data"))
        {
            sendChassisErrorResponse(res, "Missing data field in request");
            return;
        }
        
        auto json = request_json["data"];
        
        if (!json.contains("status"))
        {
            sendChassisErrorResponse(res, "status is required");
            return;
        }
        
        std::string new_status = json["status"].get<std::string>();
        
        if (db_manager_->updateSlotStatusOnly(chassis_id, slot_index, new_status))
        {
            sendChassisSuccessResponse(res, "Slot status updated successfully");
        }
        else
        {
            sendChassisErrorResponse(res, "Failed to update slot status");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理获取所有slots及其最新metrics
void HTTPServer::handleGetSlotsWithMetrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto result = db_manager_->getSlotsWithLatestMetrics();
        sendChassisSuccessResponse(res, "slots_with_metrics", result);
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理注册/更新slot信息（从body获取chassis_id和slot_index）
void HTTPServer::handleRegisterSlot(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto request_json = nlohmann::json::parse(req.body);
        
        // 检查data字段
        if (!request_json.contains("data"))
        {
            sendChassisErrorResponse(res, "Missing data field in request");
            return;
        }
        
        auto json = request_json["data"];
        
        if (!json.contains("chassis_id") || !json.contains("slot_index"))
        {
            sendChassisErrorResponse(res, "chassis_id and slot_index are required in request body");
            return;
        }
        
        if (db_manager_->saveSlot(json))
        {
            sendChassisSuccessResponse(res, "Slot registered/updated successfully");
        }
        else
        {
            sendChassisErrorResponse(res, "Failed to register/update slot");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理更新slot的metrics数据
void HTTPServer::handleUpdateSlotMetrics(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto request_json = nlohmann::json::parse(req.body);
        
        // 检查data字段
        if (!request_json.contains("data"))
        {
            sendChassisErrorResponse(res, "Missing data field in request");
            return;
        }
        
        auto json = request_json["data"];
        
        if (!json.contains("chassis_id") || !json.contains("slot_index"))
        {
            sendChassisErrorResponse(res, "chassis_id and slot_index are required in request body");
            return;
        }
        
        if (db_manager_->updateSlotMetrics(json))
        {
            sendChassisSuccessResponse(res, "Slot metrics updated successfully");
        }
        else
        {
            sendChassisErrorResponse(res, "Failed to update slot metrics");
        }
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
}

// 处理获取机箱详细列表（包含所有槽位信息）
void HTTPServer::handleGetChassisDetailedList(const httplib::Request &req, httplib::Response &res)
{
    try
    {
        auto chassis_detailed_list = db_manager_->getChassisDetailedList();
        sendChassisSuccessResponse(res, "chassis_detailed_list", chassis_detailed_list);
    }
    catch (const std::exception &e)
    {
        sendChassisExceptionResponse(res, e);
    }
} 