#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>

DatabaseManager::DatabaseManager(const std::string &db_path) : db_path_(db_path), db_(nullptr), node_monitor_running_(false)
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

        // 创建机箱表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS chassis (
                chassis_id TEXT PRIMARY KEY,
                chassis_name TEXT,
                description TEXT,
                location TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL
            )
        )");

        // 创建board表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS board (
                board_id TEXT PRIMARY KEY,
                hostname TEXT NOT NULL,
                ip_address TEXT NOT NULL,
                os_info TEXT NOT NULL,
                chassis_id TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                status TEXT NOT NULL DEFAULT 'online',
                FOREIGN KEY (chassis_id) REFERENCES chassis(chassis_id)
            )
        )");

        // 创建CPU表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS board_cpus (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                processor_id INTEGER NOT NULL,
                model_name TEXT,
                vendor TEXT,
                frequency_mhz REAL,
                cache_size TEXT,
                cores INTEGER,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id),
                UNIQUE(board_id, processor_id)
            )
        )");

        // 创建GPU表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS board_gpus (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                gpu_index INTEGER NOT NULL,
                name TEXT,
                memory_total_mb INTEGER,
                temperature_c INTEGER,
                utilization_percent INTEGER,
                gpu_type TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id),
                UNIQUE(board_id, gpu_index)
            )
        )");

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

        // 新增：初始化告警规则表
        if (!createAlarmRulesTable()) {
            std::cerr << "Failed to create alarm_rules table" << std::endl;
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

        // 获取chassis_id（如果存在）
        std::string chassis_id = board_info.contains("chassis_id") ? 
            board_info["chassis_id"].get<std::string>() : "";

        // 如果提供了chassis_id，先确保chassis表中存在对应记录
        if (!chassis_id.empty() && chassis_id != "unknown") {
            SQLite::Statement chassis_check(*db_, "SELECT chassis_id FROM chassis WHERE chassis_id = ?");
            chassis_check.bind(1, chassis_id);
            
            if (!chassis_check.executeStep()) {
                // chassis不存在，创建默认记录
                SQLite::Statement chassis_insert(*db_,
                    "INSERT INTO chassis (chassis_id, chassis_name, description, location, created_at, updated_at) "
                    "VALUES (?, ?, ?, ?, ?, ?)");
                chassis_insert.bind(1, chassis_id);
                chassis_insert.bind(2, "Chassis " + chassis_id);
                chassis_insert.bind(3, "Auto-created chassis");
                chassis_insert.bind(4, "Unknown location");
                chassis_insert.bind(5, static_cast<int64_t>(timestamp));
                chassis_insert.bind(6, static_cast<int64_t>(timestamp));
                chassis_insert.exec();
            }
        }

        // 检查Board是否已存在
        SQLite::Statement query(*db_, "SELECT board_id FROM board WHERE board_id = ?");
        query.bind(1, board_info["board_id"].get<std::string>());

        if (query.executeStep())
        {
            // Board已存在，更新信息
            SQLite::Statement update(*db_,
                                     "UPDATE board SET hostname = ?, ip_address = ?, os_info = ?, chassis_id = ?, updated_at = ? WHERE board_id = ?");
            update.bind(1, board_info["hostname"].get<std::string>());
            update.bind(2, board_info["ip_address"].get<std::string>());
            update.bind(3, board_info["os_info"].get<std::string>());
            if (!chassis_id.empty() && chassis_id != "unknown") {
                update.bind(4, chassis_id);
            } else {
                update.bind(4, nullptr);  // NULL
            }
            update.bind(5, static_cast<int64_t>(timestamp));
            update.bind(6, board_info["board_id"].get<std::string>());
            update.exec();
        }
        else
        {
            // 新Board，插入记录
            SQLite::Statement insert(*db_,
                                     "INSERT INTO board (board_id, hostname, ip_address, os_info, chassis_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert.bind(1, board_info["board_id"].get<std::string>());
            insert.bind(2, board_info["hostname"].get<std::string>());
            insert.bind(3, board_info["ip_address"].get<std::string>());
            insert.bind(4, board_info["os_info"].get<std::string>());
            if (!chassis_id.empty() && chassis_id != "unknown") {
                insert.bind(5, chassis_id);
            } else {
                insert.bind(5, nullptr);  // NULL
            }
            insert.bind(6, static_cast<int64_t>(timestamp));
            insert.bind(7, static_cast<int64_t>(timestamp));
            insert.exec();
        }

        // 保存CPU信息
        if (board_info.contains("cpu_list") && board_info["cpu_list"].is_array()) {
            saveBoardCpus(board_info["board_id"].get<std::string>(), board_info["cpu_list"]);
        }

        // 保存GPU信息
        if (board_info.contains("gpu_list") && board_info["gpu_list"].is_array()) {
            saveBoardGpus(board_info["board_id"].get<std::string>(), board_info["gpu_list"]);
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

// All metrics methods moved to database_manager_metric.cpp

// Network, Docker, and all other metrics methods moved to database_manager_metric.cpp

// Docker metrics methods moved to database_manager_metric.cpp

nlohmann::json DatabaseManager::getBoards()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询所有Board
        SQLite::Statement query(*db_, "SELECT board_id, hostname, ip_address, os_info, chassis_id, created_at, updated_at, status FROM board");

        while (query.executeStep())
        {
            nlohmann::json board;
            std::string board_id = query.getColumn(0).getString();
            
            board["board_id"] = board_id;
            board["hostname"] = query.getColumn(1).getString();
            board["ip_address"] = query.getColumn(2).getString();
            board["os_info"] = query.getColumn(3).getString();
            
            // 处理chassis_id（可能为NULL）
            if (query.getColumn(4).isNull()) {
                board["chassis_id"] = nullptr;
            } else {
                board["chassis_id"] = query.getColumn(4).getString();
            }
            
            board["created_at"] = query.getColumn(5).getInt64();
            board["updated_at"] = query.getColumn(6).getInt64();
            board["status"] = query.getColumn(7).getString();

            // 获取该board的CPU列表
            board["cpu_list"] = getBoardCpus(board_id);
            
            // 获取该board的GPU列表
            board["gpu_list"] = getBoardGpus(board_id);

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

// Get metrics methods moved to database_manager_metric.cpp

// All remaining metrics methods moved to database_manager_metric.cpp

// All metrics query methods moved to database_manager_metric.cpp


nlohmann::json DatabaseManager::getNode(const std::string &node_id)
{
    try
    {
        SQLite::Statement query(*db_, "SELECT board_id, hostname, ip_address, os_info, chassis_id, created_at, updated_at, status FROM board WHERE board_id = ?");
        query.bind(1, node_id);

        while (query.executeStep())
        {
            nlohmann::json node;
            std::string board_id = query.getColumn(0).getString();
            
            node["board_id"] = board_id;
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["os_info"] = query.getColumn(3).getString();
            
            // 处理chassis_id（可能为NULL）
            if (query.getColumn(4).isNull()) {
                node["chassis_id"] = nullptr;
            } else {
                node["chassis_id"] = query.getColumn(4).getString();
            }
            
            node["created_at"] = query.getColumn(5).getInt64();
            node["updated_at"] = query.getColumn(6).getInt64();
            node["status"] = query.getColumn(7).getString();

            // 获取该board的CPU列表
            node["cpu_list"] = getBoardCpus(board_id);
            
            // 获取该board的GPU列表
            node["gpu_list"] = getBoardGpus(board_id);

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

// 机箱管理相关方法实现

bool DatabaseManager::saveChassis(const nlohmann::json &chassis_info)
{
    try
    {
        // 检查必要字段
        if (!chassis_info.contains("chassis_id"))
        {
            return false;
        }

        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        std::string chassis_id = chassis_info["chassis_id"].get<std::string>();
        std::string chassis_name = chassis_info.contains("chassis_name") ? 
            chassis_info["chassis_name"].get<std::string>() : ("Chassis " + chassis_id);
        std::string description = chassis_info.contains("description") ? 
            chassis_info["description"].get<std::string>() : "";
        std::string location = chassis_info.contains("location") ? 
            chassis_info["location"].get<std::string>() : "";

        // 检查机箱是否已存在
        SQLite::Statement query(*db_, "SELECT chassis_id FROM chassis WHERE chassis_id = ?");
        query.bind(1, chassis_id);

        if (query.executeStep())
        {
            // 机箱已存在，更新信息
            SQLite::Statement update(*db_,
                                     "UPDATE chassis SET chassis_name = ?, description = ?, location = ?, updated_at = ? WHERE chassis_id = ?");
            update.bind(1, chassis_name);
            update.bind(2, description);
            update.bind(3, location);
            update.bind(4, static_cast<int64_t>(timestamp));
            update.bind(5, chassis_id);
            update.exec();
        }
        else
        {
            // 新机箱，插入记录
            SQLite::Statement insert(*db_,
                                     "INSERT INTO chassis (chassis_id, chassis_name, description, location, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)");
            insert.bind(1, chassis_id);
            insert.bind(2, chassis_name);
            insert.bind(3, description);
            insert.bind(4, location);
            insert.bind(5, static_cast<int64_t>(timestamp));
            insert.bind(6, static_cast<int64_t>(timestamp));
            insert.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save chassis error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateChassis(const std::string &chassis_id, const nlohmann::json &chassis_info)
{
    try
    {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        std::string chassis_name = chassis_info.contains("chassis_name") ? 
            chassis_info["chassis_name"].get<std::string>() : ("Chassis " + chassis_id);
        std::string description = chassis_info.contains("description") ? 
            chassis_info["description"].get<std::string>() : "";
        std::string location = chassis_info.contains("location") ? 
            chassis_info["location"].get<std::string>() : "";

        SQLite::Statement update(*db_,
                                 "UPDATE chassis SET chassis_name = ?, description = ?, location = ?, updated_at = ? WHERE chassis_id = ?");
        update.bind(1, chassis_name);
        update.bind(2, description);
        update.bind(3, location);
        update.bind(4, static_cast<int64_t>(timestamp));
        update.bind(5, chassis_id);
        
        return update.exec() > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update chassis error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::deleteChassis(const std::string &chassis_id)
{
    try
    {
        // 首先检查是否有board仍然关联到这个机箱
        SQLite::Statement check(*db_, "SELECT COUNT(*) FROM board WHERE chassis_id = ?");
        check.bind(1, chassis_id);
        
        if (check.executeStep() && check.getColumn(0).getInt() > 0)
        {
            std::cerr << "Cannot delete chassis " << chassis_id << ": boards are still associated with it" << std::endl;
            return false;
        }

        SQLite::Statement del(*db_, "DELETE FROM chassis WHERE chassis_id = ?");
        del.bind(1, chassis_id);
        
        return del.exec() > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Delete chassis error: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json DatabaseManager::getChassis()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, "SELECT chassis_id, chassis_name, description, location, created_at, updated_at FROM chassis ORDER BY chassis_name");

        while (query.executeStep())
        {
            nlohmann::json chassis;
            chassis["chassis_id"] = query.getColumn(0).getString();
            chassis["chassis_name"] = query.getColumn(1).isNull() ? nullptr : query.getColumn(1).getString();
            chassis["description"] = query.getColumn(2).isNull() ? nullptr : query.getColumn(2).getString();
            chassis["location"] = query.getColumn(3).isNull() ? nullptr : query.getColumn(3).getString();
            chassis["created_at"] = query.getColumn(4).getInt64();
            chassis["updated_at"] = query.getColumn(5).getInt64();

            result.push_back(chassis);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get chassis error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getChassisById(const std::string &chassis_id)
{
    try
    {
        SQLite::Statement query(*db_, "SELECT chassis_id, chassis_name, description, location, created_at, updated_at FROM chassis WHERE chassis_id = ?");
        query.bind(1, chassis_id);

        if (query.executeStep())
        {
            nlohmann::json chassis;
            chassis["chassis_id"] = query.getColumn(0).getString();
            chassis["chassis_name"] = query.getColumn(1).isNull() ? nullptr : query.getColumn(1).getString();
            chassis["description"] = query.getColumn(2).isNull() ? nullptr : query.getColumn(2).getString();
            chassis["location"] = query.getColumn(3).isNull() ? nullptr : query.getColumn(3).getString();
            chassis["created_at"] = query.getColumn(4).getInt64();
            chassis["updated_at"] = query.getColumn(5).getInt64();

            return chassis;
        }

        return nlohmann::json::object();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get chassis by id error: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

nlohmann::json DatabaseManager::getBoardsByChassis(const std::string &chassis_id)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, "SELECT board_id, hostname, ip_address, os_info, chassis_id, created_at, updated_at, status FROM board WHERE chassis_id = ? ORDER BY hostname");
        query.bind(1, chassis_id);

        while (query.executeStep())
        {
            nlohmann::json board;
            std::string board_id = query.getColumn(0).getString();
            
            board["board_id"] = board_id;
            board["hostname"] = query.getColumn(1).getString();
            board["ip_address"] = query.getColumn(2).getString();
            board["os_info"] = query.getColumn(3).getString();
            board["chassis_id"] = query.getColumn(4).getString();
            board["created_at"] = query.getColumn(5).getInt64();
            board["updated_at"] = query.getColumn(6).getInt64();
            board["status"] = query.getColumn(7).getString();

            // 获取该board的CPU列表
            board["cpu_list"] = getBoardCpus(board_id);
            
            // 获取该board的GPU列表
            board["gpu_list"] = getBoardGpus(board_id);

            result.push_back(board);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get boards by chassis error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// CPU和GPU管理相关方法实现

bool DatabaseManager::saveBoardCpus(const std::string &board_id, const nlohmann::json &cpu_list)
{
    try
    {
        if (!cpu_list.is_array()) {
            return false;
        }

        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        for (const auto &cpu : cpu_list) {
            if (!cpu.contains("processor_id")) {
                continue;
            }

            int processor_id = cpu["processor_id"].get<int>();
            std::string model_name = cpu.contains("model_name") ? 
                cpu["model_name"].get<std::string>() : "";
            std::string vendor = cpu.contains("vendor") ? 
                cpu["vendor"].get<std::string>() : "";
            double frequency_mhz = cpu.contains("frequency_mhz") ? 
                cpu["frequency_mhz"].get<double>() : 0.0;
            std::string cache_size = cpu.contains("cache_size") ? 
                cpu["cache_size"].get<std::string>() : "";
            int cores = cpu.contains("cores") ? 
                cpu["cores"].get<int>() : 0;

            // 检查CPU是否已存在
            SQLite::Statement query(*db_, "SELECT id FROM board_cpus WHERE board_id = ? AND processor_id = ?");
            query.bind(1, board_id);
            query.bind(2, processor_id);

            if (query.executeStep()) {
                // CPU已存在，更新信息
                SQLite::Statement update(*db_,
                    "UPDATE board_cpus SET model_name = ?, vendor = ?, frequency_mhz = ?, cache_size = ?, cores = ?, updated_at = ? WHERE board_id = ? AND processor_id = ?");
                update.bind(1, model_name);
                update.bind(2, vendor);
                update.bind(3, frequency_mhz);
                update.bind(4, cache_size);
                update.bind(5, cores);
                update.bind(6, static_cast<int64_t>(timestamp));
                update.bind(7, board_id);
                update.bind(8, processor_id);
                update.exec();
            } else {
                // 新CPU，插入记录
                SQLite::Statement insert(*db_,
                    "INSERT INTO board_cpus (board_id, processor_id, model_name, vendor, frequency_mhz, cache_size, cores, created_at, updated_at) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
                insert.bind(1, board_id);
                insert.bind(2, processor_id);
                insert.bind(3, model_name);
                insert.bind(4, vendor);
                insert.bind(5, frequency_mhz);
                insert.bind(6, cache_size);
                insert.bind(7, cores);
                insert.bind(8, static_cast<int64_t>(timestamp));
                insert.bind(9, static_cast<int64_t>(timestamp));
                insert.exec();
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save board CPUs error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveBoardGpus(const std::string &board_id, const nlohmann::json &gpu_list)
{
    try
    {
        if (!gpu_list.is_array()) {
            return false;
        }

        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        for (const auto &gpu : gpu_list) {
            if (!gpu.contains("index")) {
                continue;
            }

            int gpu_index = gpu["index"].get<int>();
            std::string name = gpu.contains("name") ? 
                gpu["name"].get<std::string>() : "";
            int memory_total_mb = gpu.contains("memory_total_mb") ? 
                gpu["memory_total_mb"].get<int>() : 0;
            int temperature_c = gpu.contains("temperature_c") ? 
                gpu["temperature_c"].get<int>() : 0;
            int utilization_percent = gpu.contains("utilization_percent") ? 
                gpu["utilization_percent"].get<int>() : 0;
            std::string gpu_type = gpu.contains("type") ? 
                gpu["type"].get<std::string>() : "unknown";

            // 检查GPU是否已存在
            SQLite::Statement query(*db_, "SELECT id FROM board_gpus WHERE board_id = ? AND gpu_index = ?");
            query.bind(1, board_id);
            query.bind(2, gpu_index);

            if (query.executeStep()) {
                // GPU已存在，更新信息
                SQLite::Statement update(*db_,
                    "UPDATE board_gpus SET name = ?, memory_total_mb = ?, temperature_c = ?, utilization_percent = ?, gpu_type = ?, updated_at = ? WHERE board_id = ? AND gpu_index = ?");
                update.bind(1, name);
                update.bind(2, memory_total_mb);
                update.bind(3, temperature_c);
                update.bind(4, utilization_percent);
                update.bind(5, gpu_type);
                update.bind(6, static_cast<int64_t>(timestamp));
                update.bind(7, board_id);
                update.bind(8, gpu_index);
                update.exec();
            } else {
                // 新GPU，插入记录
                SQLite::Statement insert(*db_,
                    "INSERT INTO board_gpus (board_id, gpu_index, name, memory_total_mb, temperature_c, utilization_percent, gpu_type, created_at, updated_at) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
                insert.bind(1, board_id);
                insert.bind(2, gpu_index);
                insert.bind(3, name);
                insert.bind(4, memory_total_mb);
                insert.bind(5, temperature_c);
                insert.bind(6, utilization_percent);
                insert.bind(7, gpu_type);
                insert.bind(8, static_cast<int64_t>(timestamp));
                insert.bind(9, static_cast<int64_t>(timestamp));
                insert.exec();
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save board GPUs error: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json DatabaseManager::getBoardCpus(const std::string &board_id)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, 
            "SELECT processor_id, model_name, vendor, frequency_mhz, cache_size, cores, created_at, updated_at "
            "FROM board_cpus WHERE board_id = ? ORDER BY processor_id");
        query.bind(1, board_id);

        while (query.executeStep())
        {
            nlohmann::json cpu;
            cpu["processor_id"] = query.getColumn(0).getInt();
            cpu["model_name"] = query.getColumn(1).isNull() ? nullptr : query.getColumn(1).getString();
            cpu["vendor"] = query.getColumn(2).isNull() ? nullptr : query.getColumn(2).getString();
            cpu["frequency_mhz"] = query.getColumn(3).getDouble();
            cpu["cache_size"] = query.getColumn(4).isNull() ? nullptr : query.getColumn(4).getString();
            cpu["cores"] = query.getColumn(5).getInt();
            cpu["created_at"] = query.getColumn(6).getInt64();
            cpu["updated_at"] = query.getColumn(7).getInt64();

            result.push_back(cpu);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get board CPUs error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getBoardGpus(const std::string &board_id)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, 
            "SELECT gpu_index, name, memory_total_mb, temperature_c, utilization_percent, gpu_type, created_at, updated_at "
            "FROM board_gpus WHERE board_id = ? ORDER BY gpu_index");
        query.bind(1, board_id);

        while (query.executeStep())
        {
            nlohmann::json gpu;
            gpu["index"] = query.getColumn(0).getInt();
            gpu["name"] = query.getColumn(1).isNull() ? nullptr : query.getColumn(1).getString();
            gpu["memory_total_mb"] = query.getColumn(2).getInt();
            gpu["temperature_c"] = query.getColumn(3).getInt();
            gpu["utilization_percent"] = query.getColumn(4).getInt();
            gpu["type"] = query.getColumn(5).isNull() ? nullptr : query.getColumn(5).getString();
            gpu["created_at"] = query.getColumn(6).getInt64();
            gpu["updated_at"] = query.getColumn(7).getInt64();

            result.push_back(gpu);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get board GPUs error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getAllCpus()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, 
            "SELECT bc.board_id, bc.processor_id, bc.model_name, bc.vendor, bc.frequency_mhz, bc.cache_size, bc.cores, "
            "bc.created_at, bc.updated_at, b.hostname, b.chassis_id "
            "FROM board_cpus bc "
            "JOIN board b ON bc.board_id = b.board_id "
            "ORDER BY bc.board_id, bc.processor_id");

        while (query.executeStep())
        {
            nlohmann::json cpu;
            cpu["board_id"] = query.getColumn(0).getString();
            cpu["processor_id"] = query.getColumn(1).getInt();
            cpu["model_name"] = query.getColumn(2).isNull() ? nullptr : query.getColumn(2).getString();
            cpu["vendor"] = query.getColumn(3).isNull() ? nullptr : query.getColumn(3).getString();
            cpu["frequency_mhz"] = query.getColumn(4).getDouble();
            cpu["cache_size"] = query.getColumn(5).isNull() ? nullptr : query.getColumn(5).getString();
            cpu["cores"] = query.getColumn(6).getInt();
            cpu["created_at"] = query.getColumn(7).getInt64();
            cpu["updated_at"] = query.getColumn(8).getInt64();
            cpu["hostname"] = query.getColumn(9).getString();
            cpu["chassis_id"] = query.getColumn(10).isNull() ? nullptr : query.getColumn(10).getString();

            result.push_back(cpu);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get all CPUs error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getAllGpus()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        SQLite::Statement query(*db_, 
            "SELECT bg.board_id, bg.gpu_index, bg.name, bg.memory_total_mb, bg.temperature_c, bg.utilization_percent, "
            "bg.gpu_type, bg.created_at, bg.updated_at, b.hostname, b.chassis_id "
            "FROM board_gpus bg "
            "JOIN board b ON bg.board_id = b.board_id "
            "ORDER BY bg.board_id, bg.gpu_index");

        while (query.executeStep())
        {
            nlohmann::json gpu;
            gpu["board_id"] = query.getColumn(0).getString();
            gpu["gpu_index"] = query.getColumn(1).getInt();
            gpu["name"] = query.getColumn(2).isNull() ? nullptr : query.getColumn(2).getString();
            gpu["memory_total_mb"] = query.getColumn(3).getInt();
            gpu["temperature_c"] = query.getColumn(4).getInt();
            gpu["utilization_percent"] = query.getColumn(5).getInt();
            gpu["type"] = query.getColumn(6).isNull() ? nullptr : query.getColumn(6).getString();
            gpu["created_at"] = query.getColumn(7).getInt64();
            gpu["updated_at"] = query.getColumn(8).getInt64();
            gpu["hostname"] = query.getColumn(9).getString();
            gpu["chassis_id"] = query.getColumn(10).isNull() ? nullptr : query.getColumn(10).getString();

            result.push_back(gpu);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get all GPUs error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// saveResourceUsage method moved to database_manager_metric.cpp

// All cluster metrics methods moved to database_manager_metric.cpp

// getClusterMetricsHistory method moved to database_manager_metric.cpp