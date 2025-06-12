#ifndef MANAGER_H
#define MANAGER_H

#include <string>
#include <memory>
#include "scheduler.h"

// 前向声明
class HTTPServer;
class DatabaseManager;
class BusinessManager;

/**
 * Manager类 - 管理器
 * 
 * 系统的中心管理组件，负责协调各个模块
 */
class Manager {
public:
    /**
     * 构造函数
     * 
     * @param port HTTP服务器端口
     * @param db_path 数据库文件路径
     */
    Manager(int port = 8080, const std::string& db_path = "resource_monitor.db");
    
    /**
     * 析构函数
     */
    ~Manager();
    
    /**
     * 初始化Manager
     * 
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * 启动Manager
     * 
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * 停止Manager
     */
    void stop();
    
    /**
     * 运行Manager（阻塞）
     */
    void run();

private:
    int port_;                                          // HTTP服务器端口
    std::string db_path_;                               // 数据库文件路径
    bool running_;                                      // 运行标志
    
    std::unique_ptr<HTTPServer> http_server_;           // HTTP服务器
    std::shared_ptr<DatabaseManager> db_manager_;       // 数据库管理器
    std::shared_ptr<BusinessManager> business_manager_; // 业务管理器
    std::shared_ptr<Scheduler> scheduler_; // 调度器
};

#endif // MANAGER_H
