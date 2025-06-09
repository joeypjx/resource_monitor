#ifndef MANAGER_H
#define MANAGER_H

#include <string>
#include <memory>
#include "scheduler.h"
#include "sftp_server.h"

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
     * @param sftp_port SFTP服务器端口
     * @param sftp_root_dir SFTP服务器根目录
     */
    Manager(int port = 8080, const std::string& db_path = "resource_monitor.db", int sftp_port = 2222, const std::string& sftp_root_dir = "/opt/resource_monitor/files");
    
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
    std::unique_ptr<SFTPServer> sftp_server_; // SFTP服务器
    int sftp_port_; // SFTP端口
    std::string sftp_root_dir_; // SFTP根目录
};

#endif // MANAGER_H
