#include "database_manager.h" // 假设的头文件，后续可能需要创建
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp> // 用于后续的json处理
#include <thread>

// 此函数应由 DatabaseManager 调用以初始化相关表
bool DatabaseManager::initializeChassisAndSlots() {
    if (!db_) {
        std::cerr << "Database connection not initialized in initializeChassisAndSlots." << std::endl;
        return false;
    }
    try {
        // 启用外键约束
        db_->exec("PRAGMA foreign_keys = ON;");

        // 创建 chassis 表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS chassis (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                chassis_id TEXT UNIQUE NOT NULL,
                slot_count INTEGER,
                type TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL
            );
        )");

        // 创建 slots 表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                chassis_id TEXT NOT NULL,
                slot_index INTEGER NOT NULL,
                status TEXT,
                board_type TEXT,
                board_cpu TEXT,
                board_cpu_cores INTEGER,
                board_memory BIGINT,
                board_ip TEXT,
                board_os TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                FOREIGN KEY (chassis_id) REFERENCES chassis(chassis_id) ON DELETE CASCADE,
                UNIQUE(chassis_id, slot_index)
            );
        )");

        // 创建 node 表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS node (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                box_id INTEGER NOT NULL,
                slot_id INTEGER NOT NULL,
                cpu_id INTEGER NOT NULL,
                srio_id INTEGER NOT NULL,
                hostIp TEXT NOT NULL,
                hostname TEXT NOT NULL,
                service_port INTEGER NOT NULL,
                box_type TEXT NOT NULL,
                board_type TEXT NOT NULL,
                cpu_type TEXT NOT NULL,
                os_type TEXT NOT NULL,
                resource_type TEXT NOT NULL,
                cpu_arch TEXT NOT NULL,
                gpu TEXT,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL,
                UNIQUE(box_id, slot_id, cpu_id)
            );
        )");

        std::cout << "Chassis and slots tables initialized successfully via DatabaseManager member." << std::endl;
        std::cout << "Node table initialized successfully." << std::endl;
        
        // 创建slot相关的metrics表
        
        // 创建slot_cpu_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_cpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                usage_percent REAL NOT NULL,
                load_avg_1m REAL NOT NULL,
                load_avg_5m REAL NOT NULL,
                load_avg_15m REAL NOT NULL,
                core_count INTEGER NOT NULL
            );
        )");

        // 创建slot_memory_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_memory_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL
            );
        )");

        // 创建slot_disk_metrics表（只保存汇总信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_disk_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                disk_count INTEGER NOT NULL
            );
        )");

        // 创建slot_disk_usage表（保存每个磁盘详细信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_disk_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_disk_metrics_id INTEGER NOT NULL,
                device TEXT NOT NULL,
                mount_point TEXT NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (slot_disk_metrics_id) REFERENCES slot_disk_metrics(id)
            );
        )");
        
        // 创建slot_gpu_metrics表（只保存汇总信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_gpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                gpu_count INTEGER NOT NULL
            );
        )");

        // 创建slot_gpu_usage表（保存每个GPU详细信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_gpu_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_gpu_metrics_id INTEGER NOT NULL,
                gpu_index INTEGER NOT NULL,
                name TEXT NOT NULL,
                compute_usage REAL NOT NULL,
                mem_usage REAL NOT NULL,
                mem_used BIGINT NOT NULL,
                mem_total BIGINT NOT NULL,
                temperature REAL NOT NULL,
                voltage REAL NOT NULL,
                current REAL NOT NULL,
                power REAL NOT NULL,
                FOREIGN KEY (slot_gpu_metrics_id) REFERENCES slot_gpu_metrics(id)
            );
        )");

        // 创建slot_network_metrics表（只保存汇总信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_network_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                network_count INTEGER NOT NULL
            );
        )");

        // 创建slot_network_usage表（保存每个网卡详细信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_network_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_network_metrics_id INTEGER NOT NULL,
                interface TEXT NOT NULL,
                rx_bytes BIGINT NOT NULL,
                tx_bytes BIGINT NOT NULL,
                rx_packets BIGINT NOT NULL,
                tx_packets BIGINT NOT NULL,
                rx_errors INTEGER NOT NULL,
                tx_errors INTEGER NOT NULL,
                FOREIGN KEY (slot_network_metrics_id) REFERENCES slot_network_metrics(id)
            );
        )");

        // 创建slot_docker_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_docker_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                hostIp TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                container_count INTEGER NOT NULL,
                running_count INTEGER NOT NULL,
                paused_count INTEGER NOT NULL,
                stopped_count INTEGER NOT NULL
            );
        )");

        // 创建slot_docker_containers表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS slot_docker_containers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                slot_docker_metric_id INTEGER NOT NULL,
                container_id TEXT NOT NULL,
                name TEXT NOT NULL,
                image TEXT NOT NULL,
                status TEXT NOT NULL,
                cpu_percent REAL NOT NULL,
                memory_usage BIGINT NOT NULL,
                FOREIGN KEY (slot_docker_metric_id) REFERENCES slot_docker_metrics(id)
            );
        )");

        // 创建索引以提高查询性能
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_cpu_metrics_hostIp ON slot_cpu_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_cpu_metrics_timestamp ON slot_cpu_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_memory_metrics_hostIp ON slot_memory_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_memory_metrics_timestamp ON slot_memory_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_disk_metrics_hostIp ON slot_disk_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_disk_metrics_timestamp ON slot_disk_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_network_metrics_hostIp ON slot_network_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_network_metrics_timestamp ON slot_network_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_docker_metrics_hostIp ON slot_docker_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_docker_metrics_timestamp ON slot_docker_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_gpu_metrics_hostIp ON slot_gpu_metrics(hostIp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_slot_gpu_metrics_timestamp ON slot_gpu_metrics(timestamp)");

        std::cout << "Slot metrics tables initialized successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing chassis and slot tables: " << e.what() << std::endl;
        return false;
    }
}

// 后续可以添加更多与机箱和插槽相关的数据库操作函数
// 例如: saveChassis, getChassis, saveSlot, getSlotsByChassisId 等

// 示例: 保存机箱信息 (如果机箱已存在则更新)
bool DatabaseManager::saveChassis(const nlohmann::json& chassis_info) {
    if (!db_) {
        std::cerr << "Database connection not initialized in saveChassis." << std::endl;
        return false;
    }
    if (!chassis_info.contains("chassis_id")) {
        std::cerr << "Chassis chassis_id is required." << std::endl;
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    std::string chassis_id = chassis_info["chassis_id"].get<std::string>();
    int slot_count = chassis_info.value("slot_count", 0);
    std::string type = chassis_info.value("type", "");

    try {
        SQLite::Statement query(*db_, "SELECT id FROM chassis WHERE chassis_id = ?");
        query.bind(1, chassis_id);

        if (query.executeStep()) { // Chassis exists, update it
            SQLite::Statement update(*db_, R"(
                UPDATE chassis 
                SET slot_count = ?, type = ?, updated_at = ?
                WHERE chassis_id = ?
            )");
            update.bind(1, slot_count);
            update.bind(2, type);
            update.bind(3, timestamp);
            update.bind(4, chassis_id);
            update.exec();
        } else { // Chassis does not exist, insert it
            SQLite::Statement insert(*db_, R"(
                INSERT INTO chassis (chassis_id, slot_count, type, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?)
            )");
            insert.bind(1, chassis_id);
            insert.bind(2, slot_count);
            insert.bind(3, type);
            insert.bind(4, timestamp);
            insert.bind(5, timestamp);
            insert.exec();
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving chassis: " << e.what() << std::endl;
        return false;
    }
}

// 示例: 保存插槽信息 (如果插槽已存在则更新)
bool DatabaseManager::saveSlot(const nlohmann::json& slot_info) {
    if (!db_) {
        std::cerr << "Database connection not initialized in saveSlot." << std::endl;
        return false;
    }
    if (!slot_info.contains("chassis_id") || !slot_info.contains("slot_index")) {
        std::cerr << "Chassis ID and Slot Index are required for a slot." << std::endl;
        return false;
    }

    auto now_system_clock = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now_system_clock.time_since_epoch()).count();

    std::string chassis_id = slot_info["chassis_id"].get<std::string>();
    int slot_index = slot_info["slot_index"].get<int>();
    
    try {
        // 检查 chassis 是否存在，如果不存在则创建
        SQLite::Statement chassis_check_query(*db_, "SELECT id FROM chassis WHERE chassis_id = ?");
        chassis_check_query.bind(1, chassis_id);

        if (!chassis_check_query.executeStep()) { // Chassis 不存在
            std::cout << "Chassis with chassis_id " << chassis_id << " not found. Creating it." << std::endl;
            nlohmann::json new_chassis_data;
            new_chassis_data["chassis_id"] = chassis_id;
            
            if (!this->saveChassis(new_chassis_data)) { // Call member function saveChassis
                std::cerr << "Failed to auto-create chassis with chassis_id " << chassis_id << std::endl;
                return false; 
            }
            std::cout << "Chassis with chassis_id " << chassis_id << " auto-created successfully." << std::endl;
        }

        std::string status; // This variable is declared but not really used due to logic below, can be removed.
                           // Retaining for minimal diff from previous state, but final_status is used.

        std::string board_type = slot_info.value("board_type", "");
        std::string board_cpu = slot_info.value("board_cpu", "");
        int board_cpu_cores = slot_info.value("board_cpu_cores", 0);
        long long board_memory = slot_info.value("board_memory", 0LL);
        std::string board_ip = slot_info.value("board_ip", "");
        std::string board_os = slot_info.value("board_os", "");

        SQLite::Statement query(*db_, "SELECT id FROM slots WHERE chassis_id = ? AND slot_index = ?");
        query.bind(1, chassis_id);
        query.bind(2, slot_index);

        if (query.executeStep()) { // Slot exists, update it
            std::string final_status = slot_info.contains("status") ? slot_info["status"].get<std::string>() : "online";
            SQLite::Statement update(*db_, R"(
                UPDATE slots 
                SET status = ?, board_type = ?, board_cpu = ?, board_cpu_cores = ?, board_memory = ?, board_ip = ?, board_os = ?, updated_at = ?
                WHERE chassis_id = ? AND slot_index = ?
            )");
            update.bind(1, final_status);
            update.bind(2, board_type);
            update.bind(3, board_cpu);
            update.bind(4, board_cpu_cores);
            update.bind(5, static_cast<int64_t>(board_memory));
            update.bind(6, board_ip);
            update.bind(7, board_os);
            update.bind(8, timestamp); // Always update timestamp
            update.bind(9, chassis_id);
            update.bind(10, slot_index);
            update.exec();
        } else { // Slot does not exist, insert it
            std::string final_status = slot_info.value("status", "empty"); // Default to "empty" for new slots if not specified
            SQLite::Statement insert(*db_, R"(
                INSERT INTO slots (chassis_id, slot_index, status, board_type, board_cpu, board_cpu_cores, board_memory, board_ip, board_os, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            insert.bind(1, chassis_id);
            insert.bind(2, slot_index);
            insert.bind(3, final_status);
            insert.bind(4, board_type);
            insert.bind(5, board_cpu);
            insert.bind(6, board_cpu_cores);
            insert.bind(7, static_cast<int64_t>(board_memory));
            insert.bind(8, board_ip);
            insert.bind(9, board_os);
            insert.bind(10, timestamp);
            insert.bind(11, timestamp); // created_at and updated_at are same for new entries
            insert.exec();
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving slot: " << e.what() << std::endl;
        return false;
    }
}

// Helper function to update only the status of a slot
bool DatabaseManager::updateSlotStatusOnly(const std::string& hostIp, const std::string& new_status) {
    if (!db_) {
        std::cerr << "Database connection not initialized in updateSlotStatusOnly." << std::endl;
        return false;
    }
    try {
        SQLite::Statement update(*db_, R"(
            UPDATE slots 
            SET status = ?
            WHERE hostIp = ?
        )");
        update.bind(1, new_status);
        update.bind(2, hostIp);
        int rows_affected = update.exec();
        return rows_affected > 0;
    } catch (const std::exception& e) {
        std::cerr << "Error setting slot status for host " << hostIp << ": " << e.what() << std::endl;
        return false;
    }
}

// Global runSlotStatusMonitorLoop has been removed as its logic is now in DatabaseManager::slotStatusMonitorLoop (in database_manager.cpp) 

// Slot Status Monitor Thread Management (moved from database_manager.cpp)
void DatabaseManager::startSlotStatusMonitorThread() {
    if (slot_status_monitor_running_.load()) {
        return; // Already running
    }
    slot_status_monitor_running_.store(true);
    // Use new instead of std::make_unique (C++14) for C++11 compatibility
    slot_status_monitor_thread_.reset(new std::thread(&DatabaseManager::slotStatusMonitorLoop, this));
    std::cout << "Slot status monitor thread started." << std::endl;
}

void DatabaseManager::slotStatusMonitorLoop() {
    if (!db_) {
        std::cerr << "Database connection is null for slot status monitor loop." << std::endl;
        slot_status_monitor_running_.store(false);
        return;
    }

    while (slot_status_monitor_running_.load()) {
        try {
            // Use explicit type instead of auto for C++11 compatibility
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            int64_t now_epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            SQLite::Statement query(*db_, "SELECT board_ip, updated_at, status FROM slots");
            
            while (query.executeStep()) {
                std::string hostIp = query.getColumn(0).getString();
                int64_t last_updated_at = query.getColumn(1).getInt64();
                std::string current_status = query.getColumn(2).getString();

                if (current_status != "empty" && current_status != "offline") {
                    if ((now_epoch - last_updated_at) > 5) { // 5 seconds threshold
                        std::cout << "Slot with hostIp " << hostIp 
                                  << " is inactive. Setting status to offline via DatabaseManager." << std::endl;
                        this->updateSlotStatusOnly(hostIp, "offline"); 
                    }
                }
            }
        } catch (const SQLite::Exception& e) {
            std::cerr << "SQLite error in DatabaseManager slot status monitor: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); 
        } catch (const std::exception& e) {
            std::cerr << "Error in DatabaseManager slot status monitor: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Check every second
    }
    std::cout << "DatabaseManager slot status monitor loop finished." << std::endl;
}

// Node Management Implementation

// 保存节点信息 (如果节点已存在则更新)
bool DatabaseManager::saveNode(const nlohmann::json& node_info) {
    if (!db_) {
        std::cerr << "Database connection not initialized in saveNode." << std::endl;
        return false;
    }
    
    // 检查必要字段是否存在
    if (!node_info.contains("box_id") || !node_info.contains("slot_id") || !node_info.contains("cpu_id")) {
        std::cerr << "Node box_id, slot_id and cpu_id are required." << std::endl;
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    int box_id = node_info["box_id"].get<int>();
    int slot_id = node_info["slot_id"].get<int>();
    int cpu_id = node_info["cpu_id"].get<int>();
    int srio_id = node_info.value("srio_id", 0);
    std::string hostIp = node_info.value("hostIp", "");
    std::string hostname = node_info.value("hostname", "");
    int service_port = node_info.value("service_port", 0);
    std::string box_type = node_info.value("box_type", "");
    std::string board_type = node_info.value("board_type", "");
    std::string cpu_type = node_info.value("cpu_type", "");
    std::string os_type = node_info.value("os_type", "");
    std::string resource_type = node_info.value("resource_type", "");
    std::string cpu_arch = node_info.value("cpu_arch", "");
    
    // 处理GPU信息，将JSON数组转换为字符串存储
    std::string gpu_json = "[]";
    if (node_info.contains("gpu") && node_info["gpu"].is_array()) {
        gpu_json = node_info["gpu"].dump();
    }

    try {
        SQLite::Statement query(*db_, "SELECT id FROM node WHERE box_id = ? AND slot_id = ? AND cpu_id = ?");
        query.bind(1, box_id);
        query.bind(2, slot_id);
        query.bind(3, cpu_id);

        if (query.executeStep()) { // Node exists, update it
            SQLite::Statement update(*db_, R"(
                UPDATE node 
                SET srio_id = ?, hostIp = ?, hostname = ?, service_port = ?, 
                    box_type = ?, board_type = ?, cpu_type = ?, os_type = ?, 
                    resource_type = ?, cpu_arch = ?, gpu = ?, updated_at = ?
                WHERE box_id = ? AND slot_id = ? AND cpu_id = ?
            )");
            update.bind(1, srio_id);
            update.bind(2, hostIp);
            update.bind(3, hostname);
            update.bind(4, service_port);
            update.bind(5, box_type);
            update.bind(6, board_type);
            update.bind(7, cpu_type);
            update.bind(8, os_type);
            update.bind(9, resource_type);
            update.bind(10, cpu_arch);
            update.bind(11, gpu_json);
            update.bind(12, timestamp);
            update.bind(13, box_id);
            update.bind(14, slot_id);
            update.bind(15, cpu_id);
            update.exec();
        } else { // Node does not exist, insert it
            SQLite::Statement insert(*db_, R"(
                INSERT INTO node (box_id, slot_id, cpu_id, srio_id, hostIp, hostname, service_port, 
                                box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                                gpu, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            insert.bind(1, box_id);
            insert.bind(2, slot_id);
            insert.bind(3, cpu_id);
            insert.bind(4, srio_id);
            insert.bind(5, hostIp);
            insert.bind(6, hostname);
            insert.bind(7, service_port);
            insert.bind(8, box_type);
            insert.bind(9, board_type);
            insert.bind(10, cpu_type);
            insert.bind(11, os_type);
            insert.bind(12, resource_type);
            insert.bind(13, cpu_arch);
            insert.bind(14, gpu_json);
            insert.bind(15, timestamp);
            insert.bind(16, timestamp);
            insert.exec();
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving node: " << e.what() << std::endl;
        return false;
    }
}

// 更新节点信息
bool DatabaseManager::updateNode(const nlohmann::json& node_info) {
    // 由于saveNode已经实现了更新功能，这里直接调用saveNode
    return saveNode(node_info);
}

// 根据box_id, slot_id和cpu_id获取节点信息
nlohmann::json DatabaseManager::getNode(int box_id, int slot_id, int cpu_id) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getNode." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, hostIp, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, created_at, updated_at 
            FROM node 
            WHERE box_id = ? AND slot_id = ? AND cpu_id = ?
        )");
        query.bind(1, box_id);
        query.bind(2, slot_id);
        query.bind(3, cpu_id);
        
        if (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["hostIp"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["created_at"] = query.getColumn(15).getInt64();
            node["updated_at"] = query.getColumn(16).getInt64();
            
            return node;
        }
        return nlohmann::json::object(); // 返回空对象表示未找到
    } catch (const std::exception& e) {
        std::cerr << "Error getting node: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 根据hostIp获取节点信息
nlohmann::json DatabaseManager::getNodeByHostIp(const std::string& hostIp) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getNodeByHostIp." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, hostIp, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, created_at, updated_at 
            FROM node 
            WHERE hostIp = ?
        )");
        query.bind(1, hostIp);
        
        if (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["hostIp"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["created_at"] = query.getColumn(15).getInt64();
            node["updated_at"] = query.getColumn(16).getInt64();
            
            return node;
        }
        return nlohmann::json::object(); // 返回空对象表示未找到
    } catch (const std::exception& e) {
        std::cerr << "Error getting node by hostIp: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 获取所有节点信息
nlohmann::json DatabaseManager::getAllNodes() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getAllNodes." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        SQLite::Statement query(*db_, R"(
            SELECT id, box_id, slot_id, cpu_id, srio_id, hostIp, hostname, service_port, 
                   box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, 
                   gpu, created_at, updated_at 
            FROM node 
            ORDER BY box_id, slot_id, cpu_id
        )");
        
        while (query.executeStep()) {
            nlohmann::json node;
            node["id"] = query.getColumn(0).getInt();
            node["box_id"] = query.getColumn(1).getInt();
            node["slot_id"] = query.getColumn(2).getInt();
            node["cpu_id"] = query.getColumn(3).getInt();
            node["srio_id"] = query.getColumn(4).getInt();
            node["hostIp"] = query.getColumn(5).getString();
            node["hostname"] = query.getColumn(6).getString();
            node["service_port"] = query.getColumn(7).getInt();
            node["box_type"] = query.getColumn(8).getString();
            node["board_type"] = query.getColumn(9).getString();
            node["cpu_type"] = query.getColumn(10).getString();
            node["os_type"] = query.getColumn(11).getString();
            node["resource_type"] = query.getColumn(12).getString();
            node["cpu_arch"] = query.getColumn(13).getString();
            
            // 解析GPU JSON字符串为JSON数组
            std::string gpu_json = query.getColumn(14).getString();
            try {
                node["gpu"] = nlohmann::json::parse(gpu_json);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing GPU JSON: " << e.what() << std::endl;
                node["gpu"] = nlohmann::json::array();
            }
            
            node["created_at"] = query.getColumn(15).getInt64();
            node["updated_at"] = query.getColumn(16).getInt64();
            
            result.push_back(node);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting all nodes: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// Chassis and Slot Query Methods Implementation

// 获取所有slots列表
nlohmann::json DatabaseManager::getSlots() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getSlots." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        SQLite::Statement query(*db_, R"(
            SELECT s.chassis_id, s.slot_index, s.status, s.board_type, s.board_cpu, 
                   s.board_cpu_cores, s.board_memory, s.board_ip, s.board_os, s.created_at, s.updated_at,
                   c.chassis_id as chassis_name, c.type as chassis_type
            FROM slots s
            LEFT JOIN chassis c ON s.chassis_id = c.chassis_id
            ORDER BY s.chassis_id, s.slot_index
        )");
        
        while (query.executeStep()) {
            nlohmann::json slot;
            slot["chassis_id"] = query.getColumn(0).getString();
            slot["slot_index"] = query.getColumn(1).getInt();
            slot["status"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            slot["board_type"] = query.getColumn(3).isNull() ? "" : query.getColumn(3).getString();
            slot["board_cpu"] = query.getColumn(4).isNull() ? "" : query.getColumn(4).getString();
            slot["board_cpu_cores"] = query.getColumn(5).isNull() ? 0 : query.getColumn(5).getInt();
            slot["board_memory"] = query.getColumn(6).isNull() ? 0 : query.getColumn(6).getInt64();
            slot["board_ip"] = query.getColumn(7).isNull() ? "" : query.getColumn(7).getString();
            slot["board_os"] = query.getColumn(8).isNull() ? "" : query.getColumn(8).getString();
            slot["created_at"] = query.getColumn(9).getInt64();
            slot["updated_at"] = query.getColumn(10).getInt64();
            slot["chassis_name"] = query.getColumn(11).isNull() ? "" : query.getColumn(11).getString();
            slot["chassis_type"] = query.getColumn(12).isNull() ? "" : query.getColumn(12).getString();
            
            result.push_back(slot);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting slots: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取特定slot信息
nlohmann::json DatabaseManager::getSlot(const std::string& chassis_id, int slot_index) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getSlot." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        SQLite::Statement query(*db_, R"(
            SELECT s.chassis_id, s.slot_index, s.status, s.board_type, s.board_cpu, 
                   s.board_cpu_cores, s.board_memory, s.board_ip, s.board_os, s.created_at, s.updated_at,
                   c.chassis_id as chassis_name, c.type as chassis_type
            FROM slots s
            LEFT JOIN chassis c ON s.chassis_id = c.chassis_id
            WHERE s.chassis_id = ? AND s.slot_index = ?
        )");
        query.bind(1, chassis_id);
        query.bind(2, slot_index);
        
        if (query.executeStep()) {
            nlohmann::json slot;
            slot["chassis_id"] = query.getColumn(0).getString();
            slot["slot_index"] = query.getColumn(1).getInt();
            slot["status"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            slot["board_type"] = query.getColumn(3).isNull() ? "" : query.getColumn(3).getString();
            slot["board_cpu"] = query.getColumn(4).isNull() ? "" : query.getColumn(4).getString();
            slot["board_cpu_cores"] = query.getColumn(5).isNull() ? 0 : query.getColumn(5).getInt();
            slot["board_memory"] = query.getColumn(6).isNull() ? 0 : query.getColumn(6).getInt64();
            slot["board_ip"] = query.getColumn(7).isNull() ? "" : query.getColumn(7).getString();
            slot["board_os"] = query.getColumn(8).isNull() ? "" : query.getColumn(8).getString();
            slot["created_at"] = query.getColumn(9).getInt64();
            slot["updated_at"] = query.getColumn(10).getInt64();
            slot["chassis_name"] = query.getColumn(11).isNull() ? "" : query.getColumn(11).getString();
            slot["chassis_type"] = query.getColumn(12).isNull() ? "" : query.getColumn(12).getString();
            
            return slot;
        }
        
        return nlohmann::json::object();
    } catch (const std::exception& e) {
        std::cerr << "Error getting slot: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 获取所有chassis列表
nlohmann::json DatabaseManager::getChassisList() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getChassisList." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        SQLite::Statement query(*db_, R"(
            SELECT id, chassis_id, slot_count, type, created_at, updated_at
            FROM chassis
            ORDER BY chassis_id
        )");
        
        while (query.executeStep()) {
            nlohmann::json chassis;
            chassis["id"] = query.getColumn(0).getInt();
            chassis["chassis_id"] = query.getColumn(1).getString();
            chassis["slot_count"] = query.getColumn(2).getInt();
            chassis["type"] = query.getColumn(3).isNull() ? "" : query.getColumn(3).getString();
            chassis["created_at"] = query.getColumn(4).getInt64();
            chassis["updated_at"] = query.getColumn(5).getInt64();
            
            result.push_back(chassis);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting chassis list: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取特定chassis信息，包含所有slots
nlohmann::json DatabaseManager::getChassisInfo(const std::string& chassis_id) {
    if (!db_) {
        std::cerr << "Database connection not initialized in getChassisInfo." << std::endl;
        return nlohmann::json::object();
    }
    
    try {
        nlohmann::json result;
        
        // 首先获取chassis基本信息
        SQLite::Statement chassis_query(*db_, R"(
            SELECT id, chassis_id, slot_count, type, created_at, updated_at
            FROM chassis
            WHERE chassis_id = ?
        )");
        chassis_query.bind(1, chassis_id);
        
        if (!chassis_query.executeStep()) {
            // chassis不存在
            return nlohmann::json::object();
        }
        
        result["id"] = chassis_query.getColumn(0).getInt();
        result["chassis_id"] = chassis_query.getColumn(1).getString();
        result["slot_count"] = chassis_query.getColumn(2).getInt();
        result["type"] = chassis_query.getColumn(3).isNull() ? "" : chassis_query.getColumn(3).getString();
        result["created_at"] = chassis_query.getColumn(4).getInt64();
        result["updated_at"] = chassis_query.getColumn(5).getInt64();
        
        // 然后获取该chassis的所有slots
        nlohmann::json slots = nlohmann::json::array();
        SQLite::Statement slots_query(*db_, R"(
            SELECT slot_index, status, board_type, board_cpu, board_cpu_cores, board_memory, board_ip, board_os, created_at, updated_at
            FROM slots
            WHERE chassis_id = ?
            ORDER BY slot_index
        )");
        slots_query.bind(1, chassis_id);
        
        while (slots_query.executeStep()) {
            nlohmann::json slot;
            slot["slot_index"] = slots_query.getColumn(0).getInt();
            slot["status"] = slots_query.getColumn(1).isNull() ? "" : slots_query.getColumn(1).getString();
            slot["board_type"] = slots_query.getColumn(2).isNull() ? "" : slots_query.getColumn(2).getString();
            slot["board_cpu"] = slots_query.getColumn(3).isNull() ? "" : slots_query.getColumn(3).getString();
            slot["board_cpu_cores"] = slots_query.getColumn(4).isNull() ? 0 : slots_query.getColumn(4).getInt();
            slot["board_memory"] = slots_query.getColumn(5).isNull() ? 0 : slots_query.getColumn(5).getInt64();
            slot["board_ip"] = slots_query.getColumn(6).isNull() ? "" : slots_query.getColumn(6).getString();
            slot["board_os"] = slots_query.getColumn(7).isNull() ? "" : slots_query.getColumn(7).getString();
            slot["created_at"] = slots_query.getColumn(8).getInt64();
            slot["updated_at"] = slots_query.getColumn(9).getInt64();
            
            slots.push_back(slot);
        }
        
        result["slots"] = slots;
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting chassis info: " << e.what() << std::endl;
        return nlohmann::json::object();
    }
}

// 获取所有slots信息及其最新的metrics
nlohmann::json DatabaseManager::getSlotsWithLatestMetrics() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getSlotsWithLatestMetrics." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 首先获取所有slots的基本信息
        SQLite::Statement query(*db_, R"(
            SELECT s.hostIp, s.status, s.board_type, s.board_cpu, 
                   s.board_cpu_cores, s.board_memory, s.board_ip, s.board_os, s.created_at, s.updated_at,
                   c.chassis_id as chassis_name, c.type as chassis_type
            FROM slots s
            LEFT JOIN chassis c ON s.hostIp = c.hostIp
            ORDER BY s.hostIp
        )");
        
        while (query.executeStep()) {
            nlohmann::json slot;
            
            // 基本信息
            std::string hostIp = query.getColumn(0).getString();
            
            slot["hostIp"] = hostIp;
            slot["status"] = query.getColumn(1).isNull() ? "" : query.getColumn(1).getString();
            slot["board_type"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            slot["board_cpu"] = query.getColumn(3).isNull() ? "" : query.getColumn(3).getString();
            slot["board_cpu_cores"] = query.getColumn(4).isNull() ? 0 : query.getColumn(4).getInt();
            slot["board_memory"] = query.getColumn(5).isNull() ? 0 : query.getColumn(5).getInt64();
            slot["board_ip"] = query.getColumn(6).isNull() ? "" : query.getColumn(6).getString();
            slot["board_os"] = query.getColumn(7).isNull() ? "" : query.getColumn(7).getString();
            slot["created_at"] = query.getColumn(8).getInt64();
            slot["updated_at"] = query.getColumn(9).getInt64();
            slot["chassis_name"] = query.getColumn(10).isNull() ? "" : query.getColumn(10).getString();
            slot["chassis_type"] = query.getColumn(11).isNull() ? "" : query.getColumn(11).getString();
            
            // 获取最新的CPU metrics
            SQLite::Statement cpu_query(*db_, R"(
                SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count 
                FROM slot_cpu_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            cpu_query.bind(1, hostIp);
            
            if (cpu_query.executeStep()) {
                nlohmann::json cpu_metrics;
                cpu_metrics["timestamp"] = cpu_query.getColumn(0).getInt64();
                cpu_metrics["usage_percent"] = cpu_query.getColumn(1).getDouble();
                cpu_metrics["load_avg_1m"] = cpu_query.getColumn(2).getDouble();
                cpu_metrics["load_avg_5m"] = cpu_query.getColumn(3).getDouble();
                cpu_metrics["load_avg_15m"] = cpu_query.getColumn(4).getDouble();
                cpu_metrics["core_count"] = cpu_query.getColumn(5).getInt();
                slot["latest_cpu_metrics"] = cpu_metrics;
            } else {
                slot["latest_cpu_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Memory metrics
            SQLite::Statement mem_query(*db_, R"(
                SELECT timestamp, total, used, free, usage_percent 
                FROM slot_memory_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            mem_query.bind(1, hostIp);
            
            if (mem_query.executeStep()) {
                nlohmann::json memory_metrics;
                memory_metrics["timestamp"] = mem_query.getColumn(0).getInt64();
                memory_metrics["total"] = mem_query.getColumn(1).getInt64();
                memory_metrics["used"] = mem_query.getColumn(2).getInt64();
                memory_metrics["free"] = mem_query.getColumn(3).getInt64();
                memory_metrics["usage_percent"] = mem_query.getColumn(4).getDouble();
                slot["latest_memory_metrics"] = memory_metrics;
            } else {
                slot["latest_memory_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Disk metrics
            SQLite::Statement disk_query(*db_, R"(
                SELECT id, timestamp, disk_count 
                FROM slot_disk_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            disk_query.bind(1, hostIp);
            
            if (disk_query.executeStep()) {
                nlohmann::json disk_metrics;
                long long slot_disk_metrics_id = disk_query.getColumn(0).getInt64();
                disk_metrics["timestamp"] = disk_query.getColumn(1).getInt64();
                disk_metrics["disk_count"] = disk_query.getColumn(2).getInt();
                
                // 获取磁盘详细信息
                SQLite::Statement disk_usage_query(*db_, R"(
                    SELECT device, mount_point, total, used, free, usage_percent 
                    FROM slot_disk_usage 
                    WHERE slot_disk_metrics_id = ?
                )");
                disk_usage_query.bind(1, static_cast<int64_t>(slot_disk_metrics_id));
                
                nlohmann::json disks = nlohmann::json::array();
                while (disk_usage_query.executeStep()) {
                    nlohmann::json disk;
                    disk["device"] = disk_usage_query.getColumn(0).getString();
                    disk["mount_point"] = disk_usage_query.getColumn(1).getString();
                    disk["total"] = disk_usage_query.getColumn(2).getInt64();
                    disk["used"] = disk_usage_query.getColumn(3).getInt64();
                    disk["free"] = disk_usage_query.getColumn(4).getInt64();
                    disk["usage_percent"] = disk_usage_query.getColumn(5).getDouble();
                    disks.push_back(disk);
                }
                disk_metrics["disks"] = disks;
                slot["latest_disk_metrics"] = disk_metrics;
            } else {
                slot["latest_disk_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Network metrics
            SQLite::Statement net_query(*db_, R"(
                SELECT id, timestamp, network_count 
                FROM slot_network_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            net_query.bind(1, hostIp);
            
            if (net_query.executeStep()) {
                nlohmann::json network_metrics;
                long long slot_network_metrics_id = net_query.getColumn(0).getInt64();
                network_metrics["timestamp"] = net_query.getColumn(1).getInt64();
                network_metrics["network_count"] = net_query.getColumn(2).getInt();
                
                // 获取网络接口详细信息
                SQLite::Statement net_usage_query(*db_, R"(
                    SELECT interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors 
                    FROM slot_network_usage 
                    WHERE slot_network_metrics_id = ?
                )");
                net_usage_query.bind(1, static_cast<int64_t>(slot_network_metrics_id));
                
                nlohmann::json networks = nlohmann::json::array();
                while (net_usage_query.executeStep()) {
                    nlohmann::json network;
                    network["interface"] = net_usage_query.getColumn(0).getString();
                    network["rx_bytes"] = net_usage_query.getColumn(1).getInt64();
                    network["tx_bytes"] = net_usage_query.getColumn(2).getInt64();
                    network["rx_packets"] = net_usage_query.getColumn(3).getInt64();
                    network["tx_packets"] = net_usage_query.getColumn(4).getInt64();
                    network["rx_errors"] = net_usage_query.getColumn(5).getInt();
                    network["tx_errors"] = net_usage_query.getColumn(6).getInt();
                    networks.push_back(network);
                }
                network_metrics["networks"] = networks;
                slot["latest_network_metrics"] = network_metrics;
            } else {
                slot["latest_network_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的Docker metrics
            SQLite::Statement docker_query(*db_, R"(
                SELECT id, timestamp, container_count, running_count, paused_count, stopped_count 
                FROM slot_docker_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            docker_query.bind(1, hostIp);
            
            if (docker_query.executeStep()) {
                nlohmann::json docker_metrics;
                long long slot_docker_metric_id = docker_query.getColumn(0).getInt64();
                docker_metrics["timestamp"] = docker_query.getColumn(1).getInt64();
                docker_metrics["container_count"] = docker_query.getColumn(2).getInt();
                docker_metrics["running_count"] = docker_query.getColumn(3).getInt();
                docker_metrics["paused_count"] = docker_query.getColumn(4).getInt();
                docker_metrics["stopped_count"] = docker_query.getColumn(5).getInt();
                
                // 获取容器详细信息
                SQLite::Statement container_query(*db_, R"(
                    SELECT container_id, name, image, status, cpu_percent, memory_usage 
                    FROM slot_docker_containers 
                    WHERE slot_docker_metric_id = ?
                )");
                container_query.bind(1, static_cast<int64_t>(slot_docker_metric_id));
                
                nlohmann::json containers = nlohmann::json::array();
                while (container_query.executeStep()) {
                    nlohmann::json container;
                    container["id"] = container_query.getColumn(0).getString();
                    container["name"] = container_query.getColumn(1).getString();
                    container["image"] = container_query.getColumn(2).getString();
                    container["status"] = container_query.getColumn(3).getString();
                    container["cpu_percent"] = container_query.getColumn(4).getDouble();
                    container["memory_usage"] = container_query.getColumn(5).getInt64();
                    containers.push_back(container);
                }
                docker_metrics["containers"] = containers;
                slot["latest_docker_metrics"] = docker_metrics;
            } else {
                slot["latest_docker_metrics"] = nlohmann::json::object();
            }
            
            // 获取最新的GPU metrics
            SQLite::Statement gpu_query(*db_, R"(
                SELECT id, timestamp, gpu_count 
                FROM slot_gpu_metrics 
                WHERE hostIp = ? 
                ORDER BY timestamp DESC LIMIT 1
            )");
            gpu_query.bind(1, hostIp);
            
            if (gpu_query.executeStep()) {
                nlohmann::json gpu_metrics;
                long long slot_gpu_metrics_id = gpu_query.getColumn(0).getInt64();
                gpu_metrics["timestamp"] = gpu_query.getColumn(1).getInt64();
                gpu_metrics["gpu_count"] = gpu_query.getColumn(2).getInt();
                
                // 获取GPU详细信息
                SQLite::Statement gpu_usage_query(*db_, R"(
                    SELECT gpu_index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, voltage, current, power 
                    FROM slot_gpu_usage 
                    WHERE slot_gpu_metrics_id = ?
                )");
                gpu_usage_query.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));
                
                nlohmann::json gpus = nlohmann::json::array();
                while (gpu_usage_query.executeStep()) {
                    nlohmann::json gpu;
                    gpu["index"] = gpu_usage_query.getColumn(0).getInt();
                    gpu["name"] = gpu_usage_query.getColumn(1).getString();
                    gpu["compute_usage"] = gpu_usage_query.getColumn(2).getDouble();
                    gpu["mem_usage"] = gpu_usage_query.getColumn(3).getDouble();
                    gpu["mem_used"] = gpu_usage_query.getColumn(4).getInt64();
                    gpu["mem_total"] = gpu_usage_query.getColumn(5).getInt64();
                    gpu["temperature"] = gpu_usage_query.getColumn(6).getDouble();
                    gpu["voltage"] = gpu_usage_query.getColumn(7).getDouble();
                    gpu["current"] = gpu_usage_query.getColumn(8).getDouble();
                    gpu["power"] = gpu_usage_query.getColumn(9).getDouble();
                    gpus.push_back(gpu);
                }
                gpu_metrics["gpus"] = gpus;
                slot["latest_gpu_metrics"] = gpu_metrics;
            } else {
                slot["latest_gpu_metrics"] = nlohmann::json::object();
            }
            
            result.push_back(slot);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting slots with latest metrics: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// Slot Metrics Methods Implementation

// 保存slot CPU指标
bool DatabaseManager::saveSlotCpuMetrics(const std::string& hostIp,
                                         long long timestamp, const nlohmann::json& cpu_data) {
    try {
        // 检查必要字段
        if (!cpu_data.contains("usage_percent") || !cpu_data.contains("load_avg_1m") ||
            !cpu_data.contains("load_avg_5m") || !cpu_data.contains("load_avg_15m") ||
            !cpu_data.contains("core_count")) {
            return false;
        }

        // 插入CPU指标
        SQLite::Statement insert(*db_,
            "INSERT INTO slot_cpu_metrics (hostIp, timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, hostIp);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, cpu_data["usage_percent"].get<double>());
        insert.bind(4, cpu_data["load_avg_1m"].get<double>());
        insert.bind(5, cpu_data["load_avg_5m"].get<double>());
        insert.bind(6, cpu_data["load_avg_15m"].get<double>());
        insert.bind(7, cpu_data["core_count"].get<int>());
        insert.exec();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot CPU metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot内存指标
bool DatabaseManager::saveSlotMemoryMetrics(const std::string& hostIp,
                                            long long timestamp, const nlohmann::json& memory_data) {
    try {
        // 检查必要字段
        if (!memory_data.contains("total") || !memory_data.contains("used") ||
            !memory_data.contains("free") || !memory_data.contains("usage_percent")) {
            return false;
        }

        // 插入内存指标
        SQLite::Statement insert(*db_,
            "INSERT INTO slot_memory_metrics (hostIp, timestamp, total, used, free, usage_percent) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, hostIp);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, static_cast<int64_t>(memory_data["total"].get<unsigned long long>()));
        insert.bind(4, static_cast<int64_t>(memory_data["used"].get<unsigned long long>()));
        insert.bind(5, static_cast<int64_t>(memory_data["free"].get<unsigned long long>()));
        insert.bind(6, memory_data["usage_percent"].get<double>());
        insert.exec();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot memory metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot磁盘指标
bool DatabaseManager::saveSlotDiskMetrics(const std::string& hostIp,
                                          long long timestamp, const nlohmann::json& disk_data) {
    try {
        if (!disk_data.is_array()) {
            return false;
        }

        int disk_count = disk_data.size();

        // 1. 插入slot_disk_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO slot_disk_metrics (hostIp, timestamp, disk_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, hostIp);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, disk_count);
        insert_metrics.exec();

        long long slot_disk_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个磁盘详细信息到slot_disk_usage
        for (const auto& disk : disk_data) {
            if (!disk.contains("device") || !disk.contains("mount_point") ||
                !disk.contains("total") || !disk.contains("used") ||
                !disk.contains("free") || !disk.contains("usage_percent")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO slot_disk_usage (slot_disk_metrics_id, device, mount_point, total, used, free, usage_percent) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_disk_metrics_id));
            insert_usage.bind(2, disk["device"].get<std::string>());
            insert_usage.bind(3, disk["mount_point"].get<std::string>());
            insert_usage.bind(4, static_cast<int64_t>(disk["total"].get<unsigned long long>()));
            insert_usage.bind(5, static_cast<int64_t>(disk["used"].get<unsigned long long>()));
            insert_usage.bind(6, static_cast<int64_t>(disk["free"].get<unsigned long long>()));
            insert_usage.bind(7, disk["usage_percent"].get<double>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot disk metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot网络指标
bool DatabaseManager::saveSlotNetworkMetrics(const std::string& hostIp,
                                             long long timestamp, const nlohmann::json& network_data) {
    try {
        if (!network_data.is_array()) {
            return false;
        }

        int network_count = network_data.size();

        // 1. 插入slot_network_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO slot_network_metrics (hostIp, timestamp, network_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, hostIp);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, network_count);
        insert_metrics.exec();

        long long slot_network_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个网卡详细信息到slot_network_usage
        for (const auto& network : network_data) {
            if (!network.contains("interface") || !network.contains("rx_bytes") ||
                !network.contains("tx_bytes") || !network.contains("rx_packets") ||
                !network.contains("tx_packets") || !network.contains("rx_errors") ||
                !network.contains("tx_errors")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO slot_network_usage (slot_network_metrics_id, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_network_metrics_id));
            insert_usage.bind(2, network["interface"].get<std::string>());
            insert_usage.bind(3, static_cast<int64_t>(network["rx_bytes"].get<unsigned long long>()));
            insert_usage.bind(4, static_cast<int64_t>(network["tx_bytes"].get<unsigned long long>()));
            insert_usage.bind(5, static_cast<int64_t>(network["rx_packets"].get<unsigned long long>()));
            insert_usage.bind(6, static_cast<int64_t>(network["tx_packets"].get<unsigned long long>()));
            insert_usage.bind(7, network["rx_errors"].get<int>());
            insert_usage.bind(8, network["tx_errors"].get<int>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot network metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot GPU指标
bool DatabaseManager::saveSlotGpuMetrics(const std::string& hostIp,
                                         long long timestamp, const nlohmann::json& gpu_data) {
    try {
        if (!gpu_data.is_array()) {
            return false;
        }

        int gpu_count = gpu_data.size();

        // 1. 插入slot_gpu_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO slot_gpu_metrics (hostIp, timestamp, gpu_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, hostIp);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, gpu_count);
        insert_metrics.exec();

        long long slot_gpu_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个GPU详细信息到slot_gpu_usage
        for (const auto& gpu : gpu_data) {
            if (!gpu.contains("index") || !gpu.contains("name") ||
                !gpu.contains("compute_usage") || !gpu.contains("mem_usage") ||
                !gpu.contains("mem_used") || !gpu.contains("mem_total") ||
                !gpu.contains("temperature") || !gpu.contains("voltage") ||
                !gpu.contains("current") || !gpu.contains("power")) {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO slot_gpu_usage (slot_gpu_metrics_id, gpu_index, name, compute_usage, mem_usage, "
                "mem_used, mem_total, temperature, voltage, current, power) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));
            insert_usage.bind(2, gpu["index"].get<int>());
            insert_usage.bind(3, gpu["name"].get<std::string>());
            insert_usage.bind(4, gpu["compute_usage"].get<double>());
            insert_usage.bind(5, gpu["mem_usage"].get<double>());
            insert_usage.bind(6, static_cast<int64_t>(gpu["mem_used"].get<unsigned long long>()));
            insert_usage.bind(7, static_cast<int64_t>(gpu["mem_total"].get<unsigned long long>()));
            insert_usage.bind(8, gpu["temperature"].get<double>());
            insert_usage.bind(9, gpu["voltage"].get<double>());
            insert_usage.bind(10, gpu["current"].get<double>());
            insert_usage.bind(11, gpu["power"].get<double>());
            insert_usage.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot gpu metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 保存slot Docker指标
bool DatabaseManager::saveSlotDockerMetrics(const std::string& hostIp,
                                            long long timestamp, const nlohmann::json& docker_data) {
    try {
        // 检查必要字段
        if (!docker_data.contains("container_count") || !docker_data.contains("running_count") ||
            !docker_data.contains("paused_count") || !docker_data.contains("stopped_count") ||
            !docker_data.contains("containers")) {
            return false;
        }

        // 插入Docker指标
        SQLite::Statement insert(*db_,
            "INSERT INTO slot_docker_metrics (hostIp, timestamp, container_count, running_count, paused_count, stopped_count) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, hostIp);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, docker_data["container_count"].get<int>());
        insert.bind(4, docker_data["running_count"].get<int>());
        insert.bind(5, docker_data["paused_count"].get<int>());
        insert.bind(6, docker_data["stopped_count"].get<int>());
        insert.exec();

        // 获取插入的Docker指标ID
        long long slot_docker_metric_id = db_->getLastInsertRowid();

        // 遍历所有容器
        for (const auto& container : docker_data["containers"]) {
            // 检查必要字段
            if (!container.contains("id") || !container.contains("name") ||
                !container.contains("image") || !container.contains("status") ||
                !container.contains("cpu_percent") || !container.contains("memory_usage")) {
                continue;
            }

            // 插入容器信息
            SQLite::Statement insert_container(*db_,
                "INSERT INTO slot_docker_containers (slot_docker_metric_id, container_id, name, image, status, cpu_percent, memory_usage) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_container.bind(1, static_cast<int64_t>(slot_docker_metric_id));
            insert_container.bind(2, container["id"].get<std::string>());
            insert_container.bind(3, container["name"].get<std::string>());
            insert_container.bind(4, container["image"].get<std::string>());
            insert_container.bind(5, container["status"].get<std::string>());
            insert_container.bind(6, container["cpu_percent"].get<double>());
            insert_container.bind(7, static_cast<int64_t>(container["memory_usage"].get<unsigned long long>()));
            insert_container.exec();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save slot Docker metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 获取slot CPU指标
nlohmann::json DatabaseManager::getSlotCpuMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询CPU指标
        SQLite::Statement query(*db_,
            "SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count "
            "FROM slot_cpu_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["usage_percent"] = query.getColumn(1).getDouble();
            metric["load_avg_1m"] = query.getColumn(2).getDouble();
            metric["load_avg_5m"] = query.getColumn(3).getDouble();
            metric["load_avg_15m"] = query.getColumn(4).getDouble();
            metric["core_count"] = query.getColumn(5).getInt();

            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot CPU metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot内存指标
nlohmann::json DatabaseManager::getSlotMemoryMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询内存指标
        SQLite::Statement query(*db_,
            "SELECT timestamp, total, used, free, usage_percent "
            "FROM slot_memory_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["total"] = query.getColumn(1).getInt64();
            metric["used"] = query.getColumn(2).getInt64();
            metric["free"] = query.getColumn(3).getInt64();
            metric["usage_percent"] = query.getColumn(4).getDouble();

            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot memory metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot磁盘指标
nlohmann::json DatabaseManager::getSlotDiskMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_disk_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, disk_count FROM slot_disk_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_disk_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["disk_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有磁盘详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT device, mount_point, total, used, free, usage_percent FROM slot_disk_usage WHERE slot_disk_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_disk_metrics_id));

            nlohmann::json disks = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json disk;
                disk["device"] = usage_query.getColumn(0).getString();
                disk["mount_point"] = usage_query.getColumn(1).getString();
                disk["total"] = usage_query.getColumn(2).getInt64();
                disk["used"] = usage_query.getColumn(3).getInt64();
                disk["free"] = usage_query.getColumn(4).getInt64();
                disk["usage_percent"] = usage_query.getColumn(5).getDouble();
                disks.push_back(disk);
            }
            metric["disks"] = disks;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot disk metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot网络指标
nlohmann::json DatabaseManager::getSlotNetworkMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_network_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, network_count FROM slot_network_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_network_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["network_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有网卡详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors FROM slot_network_usage WHERE slot_network_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_network_metrics_id));

            nlohmann::json networks = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json network;
                network["interface"] = usage_query.getColumn(0).getString();
                network["rx_bytes"] = usage_query.getColumn(1).getInt64();
                network["tx_bytes"] = usage_query.getColumn(2).getInt64();
                network["rx_packets"] = usage_query.getColumn(3).getInt64();
                network["tx_packets"] = usage_query.getColumn(4).getInt64();
                network["rx_errors"] = usage_query.getColumn(5).getInt();
                network["tx_errors"] = usage_query.getColumn(6).getInt();
                networks.push_back(network);
            }
            metric["networks"] = networks;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot network metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot GPU指标
nlohmann::json DatabaseManager::getSlotGpuMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询slot_gpu_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, gpu_count FROM slot_gpu_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_gpu_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["gpu_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有GPU详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT gpu_index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, voltage, current, power "
                "FROM slot_gpu_usage WHERE slot_gpu_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(slot_gpu_metrics_id));

            nlohmann::json gpus = nlohmann::json::array();
            while (usage_query.executeStep()) {
                nlohmann::json gpu;
                gpu["index"] = usage_query.getColumn(0).getInt();
                gpu["name"] = usage_query.getColumn(1).getString();
                gpu["compute_usage"] = usage_query.getColumn(2).getDouble();
                gpu["mem_usage"] = usage_query.getColumn(3).getDouble();
                gpu["mem_used"] = usage_query.getColumn(4).getInt64();
                gpu["mem_total"] = usage_query.getColumn(5).getInt64();
                gpu["temperature"] = usage_query.getColumn(6).getDouble();
                gpu["voltage"] = usage_query.getColumn(7).getDouble();
                gpu["current"] = usage_query.getColumn(8).getDouble();
                gpu["power"] = usage_query.getColumn(9).getDouble();
                gpus.push_back(gpu);
            }
            metric["gpus"] = gpus;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot gpu metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取slot Docker指标
nlohmann::json DatabaseManager::getSlotDockerMetrics(const std::string& hostIp, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();

        // 查询Docker指标
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, container_count, running_count, paused_count, stopped_count "
            "FROM slot_docker_metrics WHERE hostIp = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, hostIp);
        query.bind(2, limit);

        while (query.executeStep()) {
            nlohmann::json metric;
            long long slot_docker_metric_id = query.getColumn(0).getInt64();

            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["container_count"] = query.getColumn(2).getInt();
            metric["running_count"] = query.getColumn(3).getInt();
            metric["paused_count"] = query.getColumn(4).getInt();
            metric["stopped_count"] = query.getColumn(5).getInt();

            // 查询容器信息
            SQLite::Statement container_query(*db_,
                "SELECT container_id, name, image, status, cpu_percent, memory_usage "
                "FROM slot_docker_containers WHERE slot_docker_metric_id = ?");
            container_query.bind(1, static_cast<int64_t>(slot_docker_metric_id));

            nlohmann::json containers = nlohmann::json::array();

            while (container_query.executeStep()) {
                nlohmann::json container;
                container["id"] = container_query.getColumn(0).getString();
                container["name"] = container_query.getColumn(1).getString();
                container["image"] = container_query.getColumn(2).getString();
                container["status"] = container_query.getColumn(3).getString();
                container["cpu_percent"] = container_query.getColumn(4).getDouble();
                container["memory_usage"] = container_query.getColumn(5).getInt64();

                containers.push_back(container);
            }

            metric["containers"] = containers;
            result.push_back(metric);
        }

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get slot Docker metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 保存slot资源使用情况（综合方法）
bool DatabaseManager::saveSlotResourceUsage(const nlohmann::json& resource_usage) {
    // 检查必要字段
    if (!resource_usage.contains("hostIp") || 
        !resource_usage.contains("timestamp") || !resource_usage.contains("resource")) {
        return false;
    }
    
    std::string hostIp = resource_usage["hostIp"];
    long long timestamp = resource_usage["timestamp"];
    const auto& resource = resource_usage["resource"];
    
    // 保存各类资源数据
    if (resource.contains("cpu")) {
        saveSlotCpuMetrics(hostIp, timestamp, resource["cpu"]);
    }
    if (resource.contains("memory")) {
        saveSlotMemoryMetrics(hostIp, timestamp, resource["memory"]);
    }
    if (resource.contains("disk")) {
        saveSlotDiskMetrics(hostIp, timestamp, resource["disk"]);
    }
    if (resource.contains("network")) {
        saveSlotNetworkMetrics(hostIp, timestamp, resource["network"]);
    }
    if (resource.contains("docker")) {
        saveSlotDockerMetrics(hostIp, timestamp, resource["docker"]);
    }
    return true;
}

// 更新slot的metrics数据（综合方法）
bool DatabaseManager::updateSlotMetrics(const nlohmann::json& metrics_data) {
    // 检查必要字段
    if (!metrics_data.contains("hostIp") || 
        !metrics_data.contains("timestamp") || !metrics_data.contains("resource")) {
        std::cerr << "Missing required fields: hostIp, timestamp, resource" << std::endl;
        return false;
    }
    
    std::string hostIp = metrics_data["hostIp"];
    long long timestamp = metrics_data["timestamp"];
    const auto& resource = metrics_data["resource"];
    
    try {
        bool success = true;
        
        // 保存各类资源metrics数据
        if (resource.contains("cpu")) {
            if (!saveSlotCpuMetrics(hostIp, timestamp, resource["cpu"])) {
                std::cerr << "Failed to save CPU metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        if (resource.contains("memory")) {
            if (!saveSlotMemoryMetrics(hostIp, timestamp, resource["memory"])) {
                std::cerr << "Failed to save Memory metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        if (resource.contains("disk")) {
            if (!saveSlotDiskMetrics(hostIp, timestamp, resource["disk"])) {
                std::cerr << "Failed to save Disk metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        if (resource.contains("network")) {
            if (!saveSlotNetworkMetrics(hostIp, timestamp, resource["network"])) {
                std::cerr << "Failed to save Network metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        if (resource.contains("docker")) {
            if (!saveSlotDockerMetrics(hostIp, timestamp, resource["docker"])) {
                std::cerr << "Failed to save Docker metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        if (resource.contains("gpu")) {
            if (!saveSlotGpuMetrics(hostIp, timestamp, resource["gpu"])) {
                std::cerr << "Failed to save GPU metrics for host " << hostIp << std::endl;
                success = false;
            }
        }
        
        // 如果成功保存了metrics，则更新slot状态为online（但不修改其他slot信息）
        if (success) {
            if (!updateSlotStatusOnly(hostIp, "online")) {
                std::cerr << "Failed to update slot status to online for host " << hostIp << std::endl;
                // 不影响整体成功状态，metrics保存成功更重要
            }
        }
        
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Error updating slot metrics: " << e.what() << std::endl;
        return false;
    }
}

// 获取机箱详细列表（包含所有槽位信息）
nlohmann::json DatabaseManager::getChassisDetailedList() {
    if (!db_) {
        std::cerr << "Database connection not initialized in getChassisDetailedList." << std::endl;
        return nlohmann::json::array();
    }
    
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 首先获取所有chassis的基本信息
        SQLite::Statement chassis_query(*db_, R"(
            SELECT id, chassis_id, slot_count, type, created_at, updated_at
            FROM chassis
            ORDER BY chassis_id
        )");
        
        while (chassis_query.executeStep()) {
            nlohmann::json chassis;
            
            // 基本信息
            std::string chassis_id = chassis_query.getColumn(1).getString();
            chassis["id"] = chassis_query.getColumn(0).getInt();
            chassis["chassis_id"] = chassis_id;
            chassis["slot_count"] = chassis_query.getColumn(2).getInt();
            chassis["type"] = chassis_query.getColumn(3).isNull() ? "" : chassis_query.getColumn(3).getString();
            chassis["created_at"] = chassis_query.getColumn(4).getInt64();
            chassis["updated_at"] = chassis_query.getColumn(5).getInt64();
            
            // 获取该chassis的所有slots
            nlohmann::json slots = nlohmann::json::array();
            SQLite::Statement slots_query(*db_, R"(
                SELECT slot_index, status, board_type, board_cpu, board_cpu_cores, 
                       board_memory, board_ip, board_os, created_at, updated_at
                FROM slots
                WHERE chassis_id = ?
                ORDER BY slot_index
            )");
            slots_query.bind(1, chassis_id);
            
            while (slots_query.executeStep()) {
                nlohmann::json slot;
                slot["slot_index"] = slots_query.getColumn(0).getInt();
                slot["status"] = slots_query.getColumn(1).isNull() ? "" : slots_query.getColumn(1).getString();
                slot["board_type"] = slots_query.getColumn(2).isNull() ? "" : slots_query.getColumn(2).getString();
                slot["board_cpu"] = slots_query.getColumn(3).isNull() ? "" : slots_query.getColumn(3).getString();
                slot["board_cpu_cores"] = slots_query.getColumn(4).isNull() ? 0 : slots_query.getColumn(4).getInt();
                slot["board_memory"] = slots_query.getColumn(5).isNull() ? 0 : slots_query.getColumn(5).getInt64();
                slot["board_ip"] = slots_query.getColumn(6).isNull() ? "" : slots_query.getColumn(6).getString();
                slot["board_os"] = slots_query.getColumn(7).isNull() ? "" : slots_query.getColumn(7).getString();
                slot["created_at"] = slots_query.getColumn(8).getInt64();
                slot["updated_at"] = slots_query.getColumn(9).getInt64();
                
                slots.push_back(slot);
            }
            
            chassis["slots"] = slots;
            chassis["actual_slot_count"] = slots.size(); // 实际slot数量
            
            result.push_back(chassis);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error getting chassis detailed list: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}