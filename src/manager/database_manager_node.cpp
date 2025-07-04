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
                created_at TIMESTAMP NOT NULL
            )
        )");
        
        // 从数据库加载现有节点到内存状态地图
        SQLite::Statement query(*db_, "SELECT node_id FROM node");
        while (query.executeStep()) {
            std::string node_id = query.getColumn(0).getString();
            std::lock_guard<std::mutex> lock(node_status_mutex_);
            node_status_map_[node_id] = NodeStatus{"offline", 0};
        }

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
        if ((!node_info.contains("node_id")) || (!node_info.contains("hostname")) ||
            (!node_info.contains("ip_address")) || (!node_info.contains("os_info")) || (!node_info.contains("port")))
        {
            return false;
        }
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        
        // 检查Node是否已存在
        SQLite::Statement query(*db_, "SELECT node_id FROM node WHERE node_id = ?");
        query.bind(1, node_info["node_id"].get<std::string>());

        int gpu_count = node_info.contains("gpu_count") ? node_info["gpu_count"].get<int>() : 0;
        std::string cpu_model = node_info.contains("cpu_model") ? node_info["cpu_model"].get<std::string>() : "";

        if (query.executeStep())
        {
            // Node已存在，更新信息
            SQLite::Statement update(*db_, 
                "UPDATE node SET hostname = ?, ip_address = ?, port = ?, os_info = ?, gpu_count = ?, cpu_model = ? WHERE node_id = ?");
            update.bind(1, node_info["hostname"].get<std::string>());
            update.bind(2, node_info["ip_address"].get<std::string>());
            update.bind(3, node_info["port"].get<int>());
            update.bind(4, node_info["os_info"].get<std::string>());
            update.bind(5, gpu_count);
            update.bind(6, cpu_model);
            update.bind(7, node_info["node_id"].get<std::string>());
            update.exec();
        }
        else
        {
            // 新Node，插入记录
            auto timestamp = std::chrono::system_clock::to_time_t(now);
            SQLite::Statement insert(*db_, 
                "INSERT INTO node (node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert.bind(1, node_info["node_id"].get<std::string>());
            insert.bind(2, node_info["hostname"].get<std::string>());
            insert.bind(3, node_info["ip_address"].get<std::string>());
            insert.bind(4, node_info["port"].get<int>());
            insert.bind(5, node_info["os_info"].get<std::string>());
            insert.bind(6, gpu_count);
            insert.bind(7, cpu_model);
            insert.bind(8, static_cast<int64_t>(timestamp));
            insert.exec();

            // 同时初始化内存中的状态
            std::lock_guard<std::mutex> lock(node_status_mutex_);
            node_status_map_[node_info["node_id"].get<std::string>()] = NodeStatus{"online", static_cast<int64_t>(timestamp)};
        }
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save node error: " << e.what() << std::endl;
        return false;
    }
}

void DatabaseManager::updateNodeLastSeen(const std::string &node_id)
{
    auto now = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> lock(node_status_mutex_);
    node_status_map_[node_id] = NodeStatus{"online", static_cast<int64_t>(std::chrono::system_clock::to_time_t(now))};
}

void DatabaseManager::updateNodeStatus(const std::string &node_id, const std::string &status)
{
    std::lock_guard<std::mutex> lock(node_status_mutex_);
    if (node_status_map_.count(node_id)) {
        node_status_map_[node_id].status = status;
    }
}

NodeStatus DatabaseManager::getNodeStatus(const std::string& node_id)
{
    std::lock_guard<std::mutex> lock(node_status_mutex_);
    if (node_status_map_.count(node_id)) {
        return node_status_map_.at(node_id);
    }
    return NodeStatus{}; // 返回默认的 offline 状态
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
                auto now = std::chrono::system_clock::now();
                auto current_timestamp = std::chrono::system_clock::to_time_t(now);

                std::vector<std::string> offline_nodes;
                
                // 检查内存中的节点状态
                node_status_mutex_.lock();
                for (const auto& pair : node_status_map_) {
                    if ((pair.second.status == "online") && (current_timestamp - pair.second.updated_at > 30)) {
                        offline_nodes.push_back(pair.first);
                    }
                }
                node_status_mutex_.unlock();

                for (const auto& node_id : offline_nodes) {
                    // 获取ip用于日志 - 直接从数据库查询，避免调用getNode导致的锁问题
                    std::string ip_address = "";
                    try {
                        SQLite::Statement query(*db_, "SELECT ip_address FROM node WHERE node_id = ?");
                        query.bind(1, node_id);
                        if (query.executeStep()) {
                            ip_address = query.getColumn(0).getString();
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error getting node IP: " << e.what() << std::endl;
                    }
                    
                    if (!ip_address.empty()) {
                         LOG_INFO("Node {} ({}) is offline", node_id, ip_address);
                    } else {
                         LOG_INFO("Node {} is offline", node_id);
                    }
                   
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
        SQLite::Statement query(*db_, "SELECT node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at FROM node");

        while (query.executeStep())
        {
            nlohmann::json node = nlohmann::json::object();
            std::string node_id = query.getColumn(0).getString();
            
            node["node_id"] = node_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["port"] = query.getColumn(3).getInt();
            node["os_info"] = query.getColumn(4).getString();
            node["gpu_count"] = query.getColumn(5).getInt();
            node["cpu_model"] = query.getColumn(6).getString();
            node["created_at"] = query.getColumn(7).getInt64();
            
            // 从内存获取实时状态
            NodeStatus current_status = getNodeStatus(node_id);
            node["updated_at"] = current_status.updated_at;
            node["status"] = current_status.status;

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
        SQLite::Statement query(*db_, "SELECT node_id, hostname, ip_address, port, os_info, gpu_count, cpu_model, created_at FROM node WHERE node_id = ?");
        query.bind(1, node_id);

        if (query.executeStep())
        {
            nlohmann::json node = nlohmann::json::object();
            
            node["node_id"] = query.getColumn(0).getString();
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["port"] = query.getColumn(3).getInt();
            node["os_info"] = query.getColumn(4).getString();
            node["gpu_count"] = query.getColumn(5).getInt();
            node["cpu_model"] = query.getColumn(6).getString();
            node["created_at"] = query.getColumn(7).getInt64();
            
            // 从内存获取实时状态
            NodeStatus current_status = getNodeStatus(node_id);
            node["updated_at"] = current_status.updated_at;
            node["status"] = current_status.status;

            return node;
        }
        return {}; // 返回空json
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get node error: " << e.what() << std::endl;
        return {}; // 返回空json
    }

    return {}; // 返回空json
}

nlohmann::json DatabaseManager::getOnlineNodes()
{
    nlohmann::json online_nodes = nlohmann::json::array();
    auto all_nodes = getNodes();
    for(const auto& node : all_nodes) {
        if (node.is_null()) {
            continue;
        }
        if ((node.contains("status")) && (node["status"] == "online")) {
            online_nodes.push_back(node);
        }
    }
    return online_nodes;
} 