#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "agent_control_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

Manager::Manager(int port, const std::string& db_path)
    : db_path_(db_path), port_(port), running_(false) {
}

Manager::~Manager() {
    if (running_) {
        stop();
    }
}

bool Manager::initialize() {
    std::cout << "Initializing Manager..." << std::endl;
    
    // 创建数据库管理器
    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    if (!db_manager_->initialize()) {
        std::cerr << "Failed to initialize database manager" << std::endl;
        return false;
    }

    // 创建调度器
    scheduler_ = std::make_shared<Scheduler>(db_manager_);
    if (!scheduler_->initialize()) {
        std::cerr << "Failed to initialize scheduler" << std::endl;
        return false;
    }
    
    // 创建业务管理器
    business_manager_ = std::make_shared<BusinessManager>(db_manager_, scheduler_);
    if (!business_manager_->initialize()) {
        std::cerr << "Failed to initialize business manager" << std::endl;
        return false;
    }
    
    // 初始化模板相关的数据库表
    if (!business_manager_->initializeTables()) {
        std::cerr << "Failed to initialize template tables" << std::endl;
        return false;
    }

    // 新增：创建Agent控制管理器
    agent_control_manager_ = std::make_shared<AgentControlManager>(db_manager_);
    
    // 创建HTTP服务器
    http_server_ = std::make_unique<HTTPServer>(db_manager_, business_manager_, agent_control_manager_, port_);
    
    std::cout << "Manager initialized successfully" << std::endl;
    return true;
}

bool Manager::start() {
    if (running_) {
        std::cerr << "Manager is already running" << std::endl;
        return false;
    }
    
    std::cout << "Starting Manager..." << std::endl;
    
    // 启动HTTP服务器
    if (!http_server_->start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return false;
    }
    
    running_ = true;
    std::cout << "Manager started successfully" << std::endl;
    return true;
}

void Manager::stop() {
    if (!running_) {
        std::cerr << "Manager is not running" << std::endl;
        return;
    }
    
    std::cout << "Stopping Manager..." << std::endl;
    
    // 停止HTTP服务器
    http_server_->stop();
    
    running_ = false;
    std::cout << "Manager stopped successfully" << std::endl;
}

void Manager::run() {
    if (!initialize()) {
        std::cerr << "Failed to initialize Manager" << std::endl;
        return;
    }
    
    if (!start()) {
        std::cerr << "Failed to start Manager" << std::endl;
        return;
    }
    
    std::cout << "Manager is running. Press Ctrl+C to stop." << std::endl;
    
    // 主循环
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
