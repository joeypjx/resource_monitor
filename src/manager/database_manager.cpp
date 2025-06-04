#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>

DatabaseManager::DatabaseManager(const std::string &db_path) : db_path_(db_path), db_(nullptr), node_monitor_running_(false), slot_status_monitor_running_(false)
{
    // 构造函数，初始化数据库路径
}

DatabaseManager::~DatabaseManager()
{
    // 停止节点监控线程
    if (node_monitor_running_)
    {
        node_monitor_running_ = false;
        if (node_monitor_thread_ && node_monitor_thread_->joinable())
        {
            node_monitor_thread_->join();
        }
    }

    // 停止插槽状态监控线程
    if (slot_status_monitor_running_.load())
    {
        slot_status_monitor_running_.store(false);
        if (slot_status_monitor_thread_ && slot_status_monitor_thread_->joinable())
        {
            slot_status_monitor_thread_->join();
        }
    }
    // 数据库连接会自动关闭
}

bool DatabaseManager::initialize()
{
    try
    {
        // 创建或打开数据库
        db_ = std::make_unique<SQLite::Database>(db_path_, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        
        // 启用外键约束
        db_->exec("PRAGMA foreign_keys = ON");
        
        // 初始化Node相关的数据库表
        if (!initializeNodeTables())
        {
            std::cerr << "Node tables initialization error" << std::endl;
            return false;
        }
        
        // 初始化资源监控相关的数据库表
        if (!initializeMetricTables())
        {
            std::cerr << "Metric tables initialization error" << std::endl;
            return false;
        }
        
        // 初始化业务相关的数据库表
        if (!initializeBusinessTables())
        {
            std::cerr << "Business tables initialization error" << std::endl;
            return false;
        }

        // 初始化模板相关的数据库表
        if (!createComponentTemplateTable())
        {
            std::cerr << "Component template table initialization error" << std::endl;
            return false;
        }

        if (!createBusinessTemplateTable())
        {
            std::cerr << "Business template table initialization error" << std::endl;
            return false;
        }
        
        // 启动节点状态监控线程
        startNodeStatusMonitor();
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Database initialization error: " << e.what() << std::endl;
        return false;
    }
}