#include "database_manager.h"
#include "utils/logger.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>

// 初始化Node相关的数据库表
bool DatabaseManager::initializeNodeTables()
{
    try
    {
        // 创建node表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node (
                node_id TEXT PRIMARY KEY,
                hostname TEXT NOT NULL,
                ip_address TEXT NOT NULL,
                port INTEGER NOT NULL,
                os_info TEXT NOT NULL,
                gpu_count INTEGER DEFAULT 0,
                cpu_model TEXT DEFAULT '',
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                status TEXT NOT NULL DEFAULT 'online'
            )
        )");
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Node tables initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveNode(const nlohmann::json &node_info)
{
    try
    {
        // 检查必要字段
        if (!node_info.contains("node_id") || !node_info.contains("hostname") ||
            !node_info.contains("ip_address") || !node_info.contains("os_info") || !node_info.contains("port"))
        {
            return false;
        }
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 检查Node是否已存在
        SQLite::Statement query(*db_, "SELECT node_id FROM node WHERE node_id = ?");
        query.bind(1, node_info["node_id"].get<std::string>());

        int gpu_count = node_info.contains("gpu_count") ? node_info["gpu_count"].get<int>() : 0;
        std::string cpu_model = node_info.contains("cpu_model") ? node_info["cpu_model"].get<std::string>() : "";

        if (query.executeStep())
        {
            // Node已存在，更新信息
            SQLite::Statement update(*db_, 
                "UPDATE node SET hostname = ?, ip_address = ?, port = ?, os_info = ?, gpu_count = ?, cpu_model = ?, updated_at = ? WHERE node_id = ?");
            update.bind(1, node_info["hostname"].get<std::string>());
            update.bind(2, node_info["ip_address"].get<std::string>());
            update.bind(3, node_info["port"].get<int>());
            update.bind(4, node_info["os_info"].get<std::string>());
            update.bind(5, gpu_count);
            update.bind(6, cpu_model);
            update.bind(7, static_cast<int64_t>(timestamp));
            update.bind(8, node_info["node_id"].get<std::string>());
            update.exec();
        }
        else
        {
            // 新Node，插入记录
            SQLite::Statement insert(*db_, 
                "INSERT INTO node (node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
            insert.bind(1, node_info["node_id"].get<std::string>());
            insert.bind(2, node_info["hostname"].get<std::string>());
            insert.bind(3, node_info["ip_address"].get<std::string>());
            insert.bind(4, node_info["port"].get<int>());
            insert.bind(5, node_info["os_info"].get<std::string>());
            insert.bind(6, gpu_count);
            insert.bind(7, cpu_model);
            insert.bind(8, static_cast<int64_t>(timestamp));
            insert.bind(9, static_cast<int64_t>(timestamp));
            insert.exec();
        }
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save node error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateNodeLastSeen(const std::string &node_id)
{
    try
    {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 更新Node最后活动时间和状态为在线
        SQLite::Statement update(*db_, "UPDATE node SET updated_at = ?, status = 'online' WHERE node_id = ?");
        update.bind(1, static_cast<int64_t>(timestamp));
        update.bind(2, node_id);
        update.exec();
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update node last seen error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateNodeStatus(const std::string &node_id, const std::string &status)
{
    try
    {
        // 更新Node状态
        SQLite::Statement update(*db_, "UPDATE node SET status = ? WHERE node_id = ?");
        update.bind(1, status);
        update.bind(2, node_id);
        update.exec();
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update node status error: " << e.what() << std::endl;
        return false;
    }
}

void DatabaseManager::startNodeStatusMonitor()
{
    // 如果监控线程已经在运行，则不再启动
    if (node_monitor_running_)
    {
        return;
    }
    
    node_monitor_running_ = true;
    
    // 创建并启动监控线程
    node_monitor_thread_ = std::make_unique<std::thread>([this]()
                                                         {
        while (node_monitor_running_) {
            try {
                // 获取当前时间戳
                auto now = std::chrono::system_clock::now();
                auto current_timestamp = std::chrono::system_clock::to_time_t(now);
                
                // 查询所有节点
                SQLite::Statement query(*db_, "SELECT node_id, ip_address, updated_at FROM node where status = 'online'");
                
                while (query.executeStep()) {
                    std::string node_id = query.getColumn(0).getString();
                    std::string ip_address = query.getColumn(1).getString();
                    int64_t updated_at = query.getColumn(2).getInt64();
                    
                    // 如果超过5秒没有上报，则标记为离线
                    if (current_timestamp - updated_at > 10) {
                        LOG_INFO("Node {} is offline", ip_address);
                        updateNodeStatus(node_id, "offline");

                        // 查询node_id对应的business_components表，如果status为running，则更新为error
                        SQLite::Statement query_business_components(*db_, "SELECT component_id, status FROM business_components WHERE node_id = ?");
                        query_business_components.bind(1, node_id);
                        while (query_business_components.executeStep()) {
                            std::string component_id = query_business_components.getColumn(0).getString();
                            std::string status = query_business_components.getColumn(1).getString();
                            if (status == "running") {
                                updateComponentStatus(component_id, "error");
                            }
                        }
                    }
                }
                
                // 每秒检查一次
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (const std::exception& e) {
                std::cerr << "Node status monitor error: " << e.what() << std::endl;
                // 出错后暂停一下再继续
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        } });
}

nlohmann::json DatabaseManager::getNodes()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();
        
        // 查询所有Node
        SQLite::Statement query(*db_, "SELECT node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at, updated_at, status FROM node");

        while (query.executeStep())
        {
            nlohmann::json node;
            std::string node_id = query.getColumn(0).getString();
            
            node["node_id"] = node_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["port"] = query.getColumn(3).getInt();
            node["os_info"] = query.getColumn(4).getString();
            node["gpu_count"] = query.getColumn(5).getInt();
            node["cpu_model"] = query.getColumn(6).getString();
            node["created_at"] = query.getColumn(7).getInt64();
            node["updated_at"] = query.getColumn(8).getInt64();
            node["status"] = query.getColumn(9).getString();

            result.push_back(node);
        }
        
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get nodes error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getNode(const std::string &node_id)
{
    try
    {
        SQLite::Statement query(*db_, "SELECT node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at, updated_at, status FROM node WHERE node_id = ?");
        query.bind(1, node_id);

        while (query.executeStep())
        {
            nlohmann::json node;
            std::string node_id = query.getColumn(0).getString();
            
            node["node_id"] = node_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["port"] = query.getColumn(3).getInt();
            node["os_info"] = query.getColumn(4).getString();
            node["gpu_count"] = query.getColumn(5).getInt();
            node["cpu_model"] = query.getColumn(6).getString();
            node["created_at"] = query.getColumn(7).getInt64();
            node["updated_at"] = query.getColumn(8).getInt64();
            node["status"] = query.getColumn(9).getString();

            return node;
        }
        return nlohmann::json::array();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get node error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }

    return nlohmann::json::array();
}

nlohmann::json DatabaseManager::getOnlineNodes()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();
        // 查询所有在线Node
        SQLite::Statement query(*db_, "SELECT node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at, updated_at, status FROM node WHERE status = 'online'");
        while (query.executeStep())
        {
            nlohmann::json node;
            std::string node_id = query.getColumn(0).getString();
            node["node_id"] = node_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["port"] = query.getColumn(3).getInt();
            node["os_info"] = query.getColumn(4).getString();
            node["gpu_count"] = query.getColumn(5).getInt();
            node["cpu_model"] = query.getColumn(6).getString();
            node["created_at"] = query.getColumn(7).getInt64();
            node["updated_at"] = query.getColumn(8).getInt64();
            node["status"] = query.getColumn(9).getString();
            result.push_back(node);
        }
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get online nodes error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
} 