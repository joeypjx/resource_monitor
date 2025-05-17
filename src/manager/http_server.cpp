#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <iostream>
#include <httplib.h>
#include <regex>

HttpServer::HttpServer(int port, std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<BusinessManager> business_manager)
    : port_(port), db_manager_(db_manager), business_manager_(business_manager), running_(false), server_(nullptr) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        return true;
    }
    
    // 创建HTTP服务器
    auto server = new httplib::Server();
    server_ = server;
    
    // 设置资源监控API路由
    
    // Agent注册
    server->Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleAgentRegistration(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 资源上报
    server->Post("/api/report", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleResourceReport(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 获取所有Agent
    server->Get("/api/agents", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto response = handleGetAgents();
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 获取Agent资源数据
    server->Get(R"(/api/agents/([^/]+)/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto agent_id = req.matches[1];
            auto resource_type = req.matches[2];
            int limit = 100;
            
            if (req.has_param("limit")) {
                limit = std::stoi(req.get_param_value("limit"));
            }
            
            auto response = handleGetAgentResources(agent_id, resource_type, limit);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 设置业务部署API路由
    
    // 部署业务
    server->Post("/api/businesses", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleDeployBusiness(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 停止业务
    server->Post(R"(/api/businesses/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto business_id = req.matches[1];
            auto response = handleStopBusiness(business_id);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 重启业务
    server->Post(R"(/api/businesses/([^/]+)/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto business_id = req.matches[1];
            auto response = handleRestartBusiness(business_id);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 更新业务
    server->Put(R"(/api/businesses/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto business_id = req.matches[1];
            auto request = nlohmann::json::parse(req.body);
            auto response = handleUpdateBusiness(business_id, request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 获取所有业务
    server->Get("/api/businesses", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto response = handleGetBusinesses();
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 获取业务详情
    server->Get(R"(/api/businesses/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto business_id = req.matches[1];
            auto response = handleGetBusinessDetails(business_id);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 获取业务组件
    server->Get(R"(/api/businesses/([^/]+)/components)", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto business_id = req.matches[1];
            auto response = handleGetBusinessComponents(business_id);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 组件状态上报
    server->Post("/api/report/component", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleComponentStatusReport(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({{"status", "error"}, {"message", e.what()}}).dump(), "application/json");
        }
    });
    
    // 启动服务器
    std::cout << "Starting HTTP server on port " << port_ << std::endl;
    running_ = true;
    
    // 在新线程中启动服务器
    std::thread([this, server]() {
        server->listen("0.0.0.0", port_);
    }).detach();
    
    return true;
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping HTTP server" << std::endl;
    
    // 停止服务器
    auto server = static_cast<httplib::Server*>(server_);
    server->stop();
    
    running_ = false;
    delete server;
    server_ = nullptr;
}

nlohmann::json HttpServer::handleAgentRegistration(const nlohmann::json& request) {
    // 检查必要字段
    if (!request.contains("agent_id") || !request.contains("hostname")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    // 打印日志
    std::cout << "Agent registered: " << request.dump() << std::endl;

    // 保存Agent信息
    if (!db_manager_->saveAgent(request)) {
        return {
            {"status", "error"},
            {"message", "Failed to save agent information"}
        };
    }
    
    return {
        {"status", "success"},
        {"message", "Agent registered successfully"}
    };
}

nlohmann::json HttpServer::handleResourceReport(const nlohmann::json& request) {
    // 检查必要字段
    if (!request.contains("agent_id") || !request.contains("timestamp")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    std::string agent_id = request["agent_id"];
    long long timestamp = request["timestamp"];
    
    // 更新Agent最后一次上报时间
    db_manager_->updateAgentLastSeen(agent_id);
    
    // 保存各类资源数据
    if (request.contains("cpu")) {
        db_manager_->saveCpuMetrics(agent_id, timestamp, request["cpu"]);
    }
    
    if (request.contains("memory")) {
        db_manager_->saveMemoryMetrics(agent_id, timestamp, request["memory"]);
    }
    
    if (request.contains("disk")) {
        db_manager_->saveDiskMetrics(agent_id, timestamp, request["disk"]);
    }
    
    if (request.contains("network")) {
        db_manager_->saveNetworkMetrics(agent_id, timestamp, request["network"]);
    }
    
    if (request.contains("docker")) {
        db_manager_->saveDockerMetrics(agent_id, timestamp, request["docker"]);
    }
    
    return {
        {"status", "success"},
        {"message", "Resource data saved successfully"}
    };
}

nlohmann::json HttpServer::handleGetAgents() {
    return db_manager_->getAgents();
}

nlohmann::json HttpServer::handleGetAgentResources(const std::string& agent_id, const std::string& resource_type, int limit) {
    if (resource_type == "cpu") {
        return db_manager_->getCpuMetrics(agent_id, limit);
    } else if (resource_type == "memory") {
        return db_manager_->getMemoryMetrics(agent_id, limit);
    } else if (resource_type == "disk") {
        return db_manager_->getDiskMetrics(agent_id, limit);
    } else if (resource_type == "network") {
        return db_manager_->getNetworkMetrics(agent_id, limit);
    } else if (resource_type == "docker") {
        return db_manager_->getDockerMetrics(agent_id, limit);
    } else {
        return {
            {"status", "error"},
            {"message", "Invalid resource type"}
        };
    }
}

nlohmann::json HttpServer::handleDeployBusiness(const nlohmann::json& request) {
    return business_manager_->deployBusiness(request);
}

nlohmann::json HttpServer::handleStopBusiness(const std::string& business_id) {
    return business_manager_->stopBusiness(business_id);
}

nlohmann::json HttpServer::handleRestartBusiness(const std::string& business_id) {
    return business_manager_->restartBusiness(business_id);
}

nlohmann::json HttpServer::handleUpdateBusiness(const std::string& business_id, const nlohmann::json& request) {
    return business_manager_->updateBusiness(business_id, request);
}

nlohmann::json HttpServer::handleGetBusinesses() {
    return business_manager_->getBusinesses();
}

nlohmann::json HttpServer::handleGetBusinessDetails(const std::string& business_id) {
    return business_manager_->getBusinessDetails(business_id);
}

nlohmann::json HttpServer::handleGetBusinessComponents(const std::string& business_id) {
    return business_manager_->getBusinessComponents(business_id);
}

nlohmann::json HttpServer::handleComponentStatusReport(const nlohmann::json& request) {
    // 检查必要字段
    if (!request.contains("component_id") || !request.contains("status")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    // 更新组件状态
    if (!business_manager_->updateComponentStatus(request)) {
        return {
            {"status", "error"},
            {"message", "Failed to update component status"}
        };
    }
    
    return {
        {"status", "success"},
        {"message", "Component status updated successfully"}
    };
}
