#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "utils/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

Manager::Manager(int port, const std::string& db_path, const std::string& sftp_host)
    : port_(port)
    , db_path_(db_path)
    , sftp_host_(sftp_host)
    , running_(false)
    , http_server_(nullptr)
    , db_manager_(nullptr)
    , business_manager_(nullptr)
    , scheduler_(nullptr)
{
}

Manager::~Manager() {
    if (running_) {
        stop();
    }
}

bool Manager::initialize() {
    LOG_INFO("Initializing Manager, db_path: {}, port: {}, sftp_host: {}", db_path_, port_, sftp_host_);
    
    // 创建数据库管理器
    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    if (!db_manager_->initialize()) {
        LOG_ERROR("Failed to initialize database manager");
        return false;
    }

    // 创建调度器
    scheduler_ = std::make_shared<Scheduler>(db_manager_);
    if (!scheduler_->initialize()) {
        LOG_ERROR("Failed to initialize scheduler");
        return false;
    }
    
    // 创建业务管理器
    business_manager_ = std::make_shared<BusinessManager>(db_manager_, scheduler_, sftp_host_);
    if (!business_manager_->initialize()) {
        LOG_ERROR("Failed to initialize business manager");
        return false;
    }
    
    // 创建HTTP服务器
    http_server_ = std::make_unique<HTTPServer>(db_manager_, business_manager_, port_);
    
    
    LOG_INFO("Manager initialized successfully");
    return true;
}

bool Manager::start() {
    if (running_) {
        LOG_WARN("Manager is already running");
        return false;
    }
    
    LOG_INFO("Starting Manager...");
    
    
    // 在单独的线程中启动HTTP服务器以避免阻塞
    std::thread server_thread([this]() {
        if (!http_server_->start()) {
            LOG_ERROR("Failed to start HTTP server");
            running_ = false;
        }
    });
    server_thread.detach(); // 分离线程，让它在后台运行
    
    // 给HTTP服务器一点时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    running_ = true;
    LOG_INFO("Manager started successfully");
    return true;
}

void Manager::stop() {
    if (!running_) {
        LOG_WARN("Manager is not running");
        return;
    }
    
    LOG_INFO("Stopping Manager...");
    
    // 停止HTTP服务器
    http_server_->stop();
    
    running_ = false;
    LOG_INFO("Manager stopped successfully");
}

void Manager::run() {
    if (!initialize()) {
        LOG_ERROR("Failed to initialize Manager");
        return;
    }
    
    if (!start()) {
        LOG_ERROR("Failed to start Manager");
        return;
    }
    
    LOG_INFO("Manager is running. Press Ctrl+C to stop.");
    
    // 主循环
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
