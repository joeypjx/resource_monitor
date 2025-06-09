#include "manager.h"
#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include "sftp_server.h"
#include <iostream>
#include <thread>
#include <chrono>

Manager::Manager(int port, const std::string& db_path, int sftp_port, const std::string& sftp_root_dir)
    : db_path_(db_path), port_(port), running_(false), sftp_port_(sftp_port), sftp_root_dir_(sftp_root_dir) {
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
    
    // 创建HTTP服务器
    http_server_ = std::make_unique<HTTPServer>(db_manager_, business_manager_, port_);
    
    // // 创建SFTP服务器
    // sftp_server_ = std::make_unique<SFTPServer>(sftp_port_, sftp_root_dir_);
    
    std::cout << "Manager initialized successfully" << std::endl;
    return true;
}

bool Manager::start() {
    if (running_) {
        std::cerr << "Manager is already running" << std::endl;
        return false;
    }
    
    std::cout << "Starting Manager..." << std::endl;
    
    // // 启动SFTP服务器
    // if (sftp_server_ && !sftp_server_->start()) {
    //     std::cerr << "Failed to start SFTP server" << std::endl;
    //     return false;
    // }
    
    // 在单独的线程中启动HTTP服务器以避免阻塞
    std::thread server_thread([this]() {
        if (!http_server_->start()) {
            std::cerr << "Failed to start HTTP server" << std::endl;
            running_ = false;
        }
    });
    server_thread.detach(); // 分离线程，让它在后台运行
    
    // 给HTTP服务器一点时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
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
    
    // 停止SFTP服务器
    // if (sftp_server_) {
    //     sftp_server_->stop();
    // }
    
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
