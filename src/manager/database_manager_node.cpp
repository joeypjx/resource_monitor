#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>

// 初始化Board相关的数据库表
bool DatabaseManager::initializeBoardTables()
{
    try
    {
        // 创建board表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS board (
                board_id TEXT PRIMARY KEY,
                hostname TEXT NOT NULL,
                ip_address TEXT NOT NULL,
                os_info TEXT NOT NULL,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                status TEXT NOT NULL DEFAULT 'online'
            )
        )");
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Board tables initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveBoard(const nlohmann::json &board_info)
{
    try
    {
        // 检查必要字段
        if (!board_info.contains("board_id") || !board_info.contains("hostname") ||
            !board_info.contains("ip_address") || !board_info.contains("os_info"))
        {
            return false;
        }
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 检查Board是否已存在
        SQLite::Statement query(*db_, "SELECT board_id FROM board WHERE board_id = ?");
        query.bind(1, board_info["board_id"].get<std::string>());

        if (query.executeStep())
        {
            // Board已存在，更新信息
            SQLite::Statement update(*db_, 
                                     "UPDATE board SET hostname = ?, ip_address = ?, os_info = ?, updated_at = ? WHERE board_id = ?");
            update.bind(1, board_info["hostname"].get<std::string>());
            update.bind(2, board_info["ip_address"].get<std::string>());
            update.bind(3, board_info["os_info"].get<std::string>());
            update.bind(4, static_cast<int64_t>(timestamp));
            update.bind(5, board_info["board_id"].get<std::string>());
            update.exec();
        }
        else
        {
            // 新Board，插入记录
            SQLite::Statement insert(*db_, 
                                     "INSERT INTO board (board_id, hostname, ip_address, os_info, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)");
            insert.bind(1, board_info["board_id"].get<std::string>());
            insert.bind(2, board_info["hostname"].get<std::string>());
            insert.bind(3, board_info["ip_address"].get<std::string>());
            insert.bind(4, board_info["os_info"].get<std::string>());
            insert.bind(5, static_cast<int64_t>(timestamp));
            insert.bind(6, static_cast<int64_t>(timestamp));
            insert.exec();
        }
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save board error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateBoardLastSeen(const std::string &board_id)
{
    try
    {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 更新Board最后活动时间和状态为在线
        SQLite::Statement update(*db_, "UPDATE board SET updated_at = ?, status = 'online' WHERE board_id = ?");
        update.bind(1, static_cast<int64_t>(timestamp));
        update.bind(2, board_id);
        update.exec();
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update board last seen error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateBoardStatus(const std::string &board_id, const std::string &status)
{
    try
    {
        // 更新Board状态
        SQLite::Statement update(*db_, "UPDATE board SET status = ? WHERE board_id = ?");
        update.bind(1, status);
        update.bind(2, board_id);
        update.exec();
        
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update board status error: " << e.what() << std::endl;
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
                SQLite::Statement query(*db_, "SELECT board_id, updated_at FROM board");
                
                while (query.executeStep()) {
                    std::string board_id = query.getColumn(0).getString();
                    int64_t updated_at = query.getColumn(1).getInt64();
                    
                    // 如果超过5秒没有上报，则标记为离线
                    if (current_timestamp - updated_at > 5) {
                        updateBoardStatus(board_id, "offline");
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

nlohmann::json DatabaseManager::getBoards()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();
        
        // 查询所有Board
        SQLite::Statement query(*db_, "SELECT board_id, hostname, ip_address, os_info, created_at, updated_at, status FROM board");

        while (query.executeStep())
        {
            nlohmann::json board;
            std::string board_id = query.getColumn(0).getString();
            
            board["board_id"] = board_id;
            board["hostname"] = query.getColumn(1).getString();
            board["ip_address"] = query.getColumn(2).getString();
            board["os_info"] = query.getColumn(3).getString();
            board["created_at"] = query.getColumn(4).getInt64();
            board["updated_at"] = query.getColumn(5).getInt64();
            board["status"] = query.getColumn(6).getString();

            result.push_back(board);
        }
        
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get boards error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getBoard(const std::string &board_id)
{
    try
    {
        SQLite::Statement query(*db_, "SELECT board_id, hostname, ip_address, os_info, created_at, updated_at, status FROM board WHERE board_id = ?");
        query.bind(1, board_id);

        while (query.executeStep())
        {
            nlohmann::json node;
            std::string board_id = query.getColumn(0).getString();
            
            node["board_id"] = board_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["os_info"] = query.getColumn(3).getString();
            node["created_at"] = query.getColumn(4).getInt64();
            node["updated_at"] = query.getColumn(5).getInt64();
            node["status"] = query.getColumn(6).getString();

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