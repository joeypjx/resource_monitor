#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>
#include "manager/manager.h"
#include "utils/logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

// 全局Manager实例，用于信号处理
std::unique_ptr<Manager> g_manager;

// @brief 信号处理函数
// 
// @param signum 信号量
// 优雅地停止Manager服务。
void signalHandler(int signum) {
    LOG_INFO("Received signal {}", signum);
    if (g_manager) {
        g_manager->stop();
    }
    exit(signum);
}

// @brief Manager主程序入口
// 
// @param argc 参数个数
// @param argv 参数列表
// @return int 退出码
// 
// Manager作为中心控制器，负责接收Agent上报的数据，
// 提供Web界面用于展示和管理，以及响应API请求。
// 支持通过命令行参数进行配置。
int main(int argc, char* argv[]) {
    // 初始化日志
    Logger::initialize("manager", "manager.log");

    // 默认参数
    int port = 8080; // HTTP服务端口
    std::string db_path = "resource_monitor.db"; // SQLite数据库文件路径
    std::string sftp_host = ""; // SFTP服务器地址，用于二进制包管理
    std::string config_path = "manager_config.json"; // 默认配置文件路径

    // 先扫描是否有 --config 参数，提前加载配置文件
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }
    // 读取配置文件（如果存在）
    std::ifstream config_ifs(config_path);
    if (config_ifs) {
        try {
            nlohmann::json config_json;
            config_ifs >> config_json;
            if (config_json.contains("port")) port = config_json["port"].get<int>();
            if (config_json.contains("db_path")) db_path = config_json["db_path"].get<std::string>();
            if (config_json.contains("sftp_host")) sftp_host = config_json["sftp_host"].get<std::string>();
            LOG_INFO("Loaded config from %s", config_path.c_str());
        } catch (const std::exception& e) {
            LOG_WARN("Failed to parse config file %s: %s", config_path.c_str(), e.what());
        }
    } else {
        LOG_INFO("Config file %s not found, using defaults and command line", config_path.c_str());
    }

    // 解析命令行参数，覆盖配置文件
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--db-path" && i + 1 < argc) {
            db_path = argv[++i];
        } else if (arg == "--sftp-host" && i + 1 < argc) {
            sftp_host = argv[++i];
        } else if (arg == "--help") {
            LOG_INFO("Usage: manager [options]");
            LOG_INFO("Options:");
            LOG_INFO("  --port <port>       HTTP server port (default: 8080)");
            LOG_INFO("  --db-path <path>    Database file path (default: resource_monitor.db)");
            LOG_INFO("  --sftp-host <host>  SFTP host (like sftp://root:password@192.168.10.15:22/data/)");
            LOG_INFO("  --config <file>     JSON config file (default: manager_config.json)");
            LOG_INFO("  --help              Show this help message");
            return 0;
        }
    }
    
    // 注册信号处理，用于优雅退出
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建Manager实例
    g_manager = std::make_unique<Manager>(port, db_path, sftp_host);
    
    // 初始化Manager
    if (!g_manager->initialize()) {
        LOG_ERROR("Failed to initialize manager");
        return 1;
    }
    
    // 启动Manager服务
    if (!g_manager->start()) {
        LOG_ERROR("Failed to start manager");
        return 1;
    }
    
    LOG_INFO("Manager started on port {}", port);
    LOG_INFO("Press Ctrl+C to stop...");
    
    // 主线程阻塞，等待信号
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
