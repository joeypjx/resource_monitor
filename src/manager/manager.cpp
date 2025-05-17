#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

Manager::Manager(int port, const std::string& db_path)
    : port_(port), db_path_(db_path), running_(false) {
}

Manager::~Manager() {
    stop();
}

bool Manager::start() {
    if (running_) {
        return true;
    }
    
    std::cout << "Starting Manager..." << std::endl;
    
    // 创建数据库管理器
    db_manager_ = std::make_shared<DatabaseManager>(db_path_);
    
    // 初始化数据库
    if (!db_manager_->initialize()) {
        std::cerr << "Failed to initialize database" << std::endl;
        return false;
    }
    
    // 创建调度器
    scheduler_ = std::make_shared<Scheduler>(db_manager_);
    
    // 创建业务管理器
    business_manager_ = std::make_shared<BusinessManager>(db_manager_, scheduler_);
    
    // 初始化业务管理器
    if (!business_manager_->initialize()) {
        std::cerr << "Failed to initialize business manager" << std::endl;
        return false;
    }
    
    // 创建HTTP服务器
    http_server_ = std::make_unique<HttpServer>(port_, db_manager_, business_manager_);
    
    // 启动HTTP服务器
    if (!http_server_->start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return false;
    }
    
    running_ = true;
    
    return true;
}

void Manager::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping Manager..." << std::endl;
    
    // 停止HTTP服务器
    if (http_server_) {
        http_server_->stop();
    }
    
    running_ = false;
}

void Manager::run() {
    if (!start()) {
        return;
    }
    
    std::cout << "Manager is running. Press Ctrl+C to stop." << std::endl;
    
    // 主循环
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
