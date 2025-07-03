#ifndef MANAGER_H
#define MANAGER_H

#include <string>   // for std::string
#include <memory>   // for std::unique_ptr, std::shared_ptr
#include "scheduler.h" // for Scheduler

// 前向声明 (Forward declarations)
// 避免在头文件中引入完整的类定义，减少编译依赖
class HTTPServer;       // HTTP服务器，负责处理API请求和Web界面
class DatabaseManager;  // 数据库管理器，负责所有数据库操作
class BusinessManager;  // 业务逻辑管理器，处理核心业务逻辑

// @class Manager
// @brief 系统的中心管理组件 (Central Management Component)
// 
// Manager是整个监控系统的核心控制器。它聚合了多个关键模块：
// 1. HTTPServer: 用于接收Agent上报的数据、处理来自Web前端的API请求。
// 2. DatabaseManager: 负责将数据持久化到SQLite数据库。
// 3. BusinessManager: 封装了系统的核心业务逻辑，如节点管理、任务管理等。
// 4. Scheduler: 调度器，用于执行周期性或一次性的后台任务。
// 
// Manager负责这些模块的生命周期管理（初始化、启动、停止）。
class Manager {
public:
    // @brief 默认构造函数
    // 使用默认参数构造Manager。
    Manager() = default;
    
    // @brief 构造函数
    // 
    // @param port HTTP服务器要监听的端口
    // @param db_path SQLite数据库文件的路径
    // @param sftp_host SFTP服务器的地址，用于二进制文件分发
    Manager(int port = 8080, const std::string& db_path = "resource_monitor.db", const std::string& sftp_host = "");
    
    // @brief 析构函数
    // 自动停止Manager并清理资源。
    ~Manager();
    
    // @brief 初始化Manager
    // 依次初始化数据库管理器、业务逻辑管理器和HTTP服务器。
    // @return bool 如果所有模块都成功初始化，则返回true。
    bool initialize();
    
    // @brief 启动Manager
    // 启动HTTP服务器和调度器。
    // @return bool 如果成功启动，则返回true。
    bool start();
    
    // @brief 停止Manager
    // 优雅地停止所有服务和后台任务。
    void stop();
    
    // @brief 运行Manager（阻塞式）
    // 这是一个阻塞方法，它会保持Manager持续运行，直到被外部信号（如Ctrl+C）中断。
    // 内部实现通常是一个无限循环。
    void run();

private:
    // --- 配置信息 ---
    int port_;                  // HTTP服务器端口
    std::string db_path_;       // 数据库文件路径
    std::string sftp_host_;     // SFTP服务器地址

    // --- 状态信息 ---
    bool running_;              // Manager是否正在运行的标志
    
    // --- 核心组件 ---
    std::unique_ptr<HTTPServer> http_server_;           // HTTP服务器实例
    std::shared_ptr<DatabaseManager> db_manager_;       // 数据库管理器实例
    std::shared_ptr<BusinessManager> business_manager_; // 业务逻辑管理器实例
    std::shared_ptr<Scheduler> scheduler_;              // 任务调度器实例
};

#endif // MANAGER_H
