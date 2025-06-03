#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>

// 扩展数据库管理器，添加业务部署相关的方法

// 初始化业务相关的数据库表
bool DatabaseManager::initializeBusinessTables() {
    try {
        // 创建businesses表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS businesses (
                business_id TEXT PRIMARY KEY,
                business_name TEXT NOT NULL,
                status TEXT NOT NULL,
                created_at TIMESTAMP NOT NULL,
                updated_at TIMESTAMP NOT NULL
            )
        )");
        
        // 创建business_components表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS business_components (
                component_id TEXT PRIMARY KEY,
                business_id TEXT NOT NULL,
                component_name TEXT NOT NULL,
                type TEXT NOT NULL,
                image_url TEXT,
                image_name TEXT,
                binary_path TEXT,
                binary_url TEXT,
                process_id TEXT,
                resource_requirements TEXT NOT NULL,
                environment_variables TEXT,
                config_files TEXT,
                affinity TEXT,
                node_id TEXT,
                container_id TEXT,
                status TEXT NOT NULL,
                started_at TIMESTAMP,
                updated_at TIMESTAMP,
                FOREIGN KEY (business_id) REFERENCES businesses(business_id),
                FOREIGN KEY (node_id) REFERENCES board(board_id)
            )
        )");
        
        // 创建component_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS component_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                component_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                cpu_percent REAL NOT NULL,
                memory_mb INTEGER NOT NULL,
                gpu_percent REAL,
                FOREIGN KEY (component_id) REFERENCES business_components(component_id)
            )
        )");
        
        // 创建索引以提高查询性能
        db_->exec("CREATE INDEX IF NOT EXISTS idx_business_components_business_id ON business_components(business_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_business_components_node_id ON business_components(node_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_component_metrics_component_id ON component_metrics(component_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_component_metrics_timestamp ON component_metrics(timestamp)");
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Business tables initialization error: " << e.what() << std::endl;
        return false;
    }
}

// 保存业务信息
bool DatabaseManager::saveBusiness(const nlohmann::json& business_info) {
    try {
        // 检查必要字段
        if (!business_info.contains("business_id") || !business_info.contains("business_name") || 
            !business_info.contains("status")) {
            return false;
        }
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 检查业务是否已存在
        SQLite::Statement query(*db_, "SELECT business_id FROM businesses WHERE business_id = ?");
        query.bind(1, business_info["business_id"].get<std::string>());
        
        if (query.executeStep()) {
            // 业务已存在，更新信息
            SQLite::Statement update(*db_, 
                "UPDATE businesses SET business_name = ?, status = ?, updated_at = ? WHERE business_id = ?");
            update.bind(1, business_info["business_name"].get<std::string>());
            update.bind(2, business_info["status"].get<std::string>());
            update.bind(3, static_cast<int64_t>(timestamp));
            update.bind(4, business_info["business_id"].get<std::string>());
            update.exec();
        } else {
            // 新业务，插入记录
            SQLite::Statement insert(*db_, 
                "INSERT INTO businesses (business_id, business_name, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?)");
            insert.bind(1, business_info["business_id"].get<std::string>());
            insert.bind(2, business_info["business_name"].get<std::string>());
            insert.bind(3, business_info["status"].get<std::string>());
            insert.bind(4, static_cast<int64_t>(timestamp));
            insert.bind(5, static_cast<int64_t>(timestamp));
            insert.exec();
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save business error: " << e.what() << std::endl;
        return false;
    }
}

// 更新业务状态
bool DatabaseManager::updateBusinessStatus(const std::string& business_id, const std::string& status) {
    try {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 更新业务状态
        SQLite::Statement update(*db_, "UPDATE businesses SET status = ?, updated_at = ? WHERE business_id = ?");
        update.bind(1, status);
        update.bind(2, static_cast<int64_t>(timestamp));
        update.bind(3, business_id);
        update.exec();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Update business status error: " << e.what() << std::endl;
        return false;
    }
}

// 保存业务组件信息
bool DatabaseManager::saveBusinessComponent(const nlohmann::json& component_info) {
    try {
        
        // 检查必要字段
        if (!component_info.contains("component_id") || !component_info.contains("business_id") || 
            !component_info.contains("component_name") || !component_info.contains("type") || 
            !component_info.contains("resource_requirements") || !component_info.contains("status") ) {
            return false;
        }
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 检查组件是否已存在
        SQLite::Statement query(*db_, "SELECT component_id FROM business_components WHERE component_id = ?");
        query.bind(1, component_info["component_id"].get<std::string>());
        
        // 准备JSON字段
        std::string resource_requirements = component_info["resource_requirements"].dump();
        std::string environment_variables = component_info.contains("environment_variables") ? 
            component_info["environment_variables"].dump() : "{}";
        std::string config_files = component_info.contains("config_files") ? 
            component_info["config_files"].dump() : "[]";
        std::string affinity = component_info.contains("affinity") ? 
            component_info["affinity"].dump() : "{}";
        
        if (query.executeStep()) {
            // 组件已存在，更新信息
            SQLite::Statement update(*db_, 
                "UPDATE business_components SET "
                "business_id = ?, component_name = ?, type = ?, image_url = ?, image_name = ?, "
                "resource_requirements = ?, environment_variables = ?, config_files = ?, affinity = ?, "
                "node_id = ?, container_id = ?, status = ?, updated_at = ? , binary_path = ?, binary_url = ?, process_id = ?"
                "WHERE component_id = ?");
            
            int idx = 1;
            update.bind(idx++, component_info["business_id"].get<std::string>());
            update.bind(idx++, component_info["component_name"].get<std::string>());
            update.bind(idx++, component_info["type"].get<std::string>());
            update.bind(idx++, component_info.contains("image_url") ? component_info["image_url"].get<std::string>() : "");
            update.bind(idx++, component_info.contains("image_name") ? component_info["image_name"].get<std::string>() : "");
            update.bind(idx++, resource_requirements);
            update.bind(idx++, environment_variables);
            update.bind(idx++, config_files);
            update.bind(idx++, affinity);
            update.bind(idx++, component_info.contains("node_id") ? component_info["node_id"].get<std::string>() : "");
            update.bind(idx++, component_info.contains("container_id") ? component_info["container_id"].get<std::string>() : "");
            update.bind(idx++, component_info["status"].get<std::string>());
            update.bind(idx++, static_cast<int64_t>(timestamp));
            update.bind(idx++, component_info["component_id"].get<std::string>());
            update.bind(idx++, component_info.contains("binary_path") ? component_info["binary_path"].get<std::string>() : "");
            update.bind(idx++, component_info.contains("binary_url") ? component_info["binary_url"].get<std::string>() : "");
            update.bind(idx++, component_info.contains("process_id") ? component_info["process_id"].get<std::string>() : "");
            
            update.exec();
        } else {
            // 新组件，插入记录
            SQLite::Statement insert(*db_, 
                "INSERT INTO business_components ("
                "component_id, business_id, component_name, type, image_url, image_name, "
                "resource_requirements, environment_variables, config_files, affinity, "
                "node_id, container_id, status, started_at, updated_at, binary_path, binary_url, process_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            
            int idx = 1;
            insert.bind(idx++, component_info["component_id"].get<std::string>());
            insert.bind(idx++, component_info["business_id"].get<std::string>());
            insert.bind(idx++, component_info["component_name"].get<std::string>());
            insert.bind(idx++, component_info["type"].get<std::string>());
            insert.bind(idx++, component_info.contains("image_url") ? component_info["image_url"].get<std::string>() : "");
            insert.bind(idx++, component_info.contains("image_name") ? component_info["image_name"].get<std::string>() : "");
            insert.bind(idx++, resource_requirements);
            insert.bind(idx++, environment_variables);
            insert.bind(idx++, config_files);
            insert.bind(idx++, affinity);
            insert.bind(idx++, component_info.contains("node_id") ? component_info["node_id"].get<std::string>() : "");
            insert.bind(idx++, component_info.contains("container_id") ? component_info["container_id"].get<std::string>() : "");
            insert.bind(idx++, component_info["status"].get<std::string>());
            insert.bind(idx++, static_cast<int64_t>(timestamp));
            insert.bind(idx++, static_cast<int64_t>(timestamp));
            insert.bind(idx++, component_info.contains("binary_path") ? component_info["binary_path"].get<std::string>() : "");
            insert.bind(idx++, component_info.contains("binary_url") ? component_info["binary_url"].get<std::string>() : "");
            insert.bind(idx++, component_info.contains("process_id") ? component_info["process_id"].get<std::string>() : "");
            
            insert.exec();
        }

        // 打印组件信息
        std::cout << "2 save Component info: " << component_info.dump() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save business component error: " << e.what() << std::endl;
        return false;
    }
}

// 更新业务组件状态
bool DatabaseManager::updateComponentStatus(const std::string& component_id, 
                                          const std::string& status, 
                                          const std::string& container_id) {
    try {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        // 更新组件状态
        SQLite::Statement update(*db_, 
            "UPDATE business_components SET status = ?, container_id = ?, updated_at = ? WHERE component_id = ?");
        update.bind(1, status);
        update.bind(2, container_id);
        update.bind(3, static_cast<int64_t>(timestamp));
        update.bind(4, component_id);
        update.exec();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Update component status error: " << e.what() << std::endl;
        return false;
    }
}

// 保存组件资源使用指标
bool DatabaseManager::saveComponentMetrics(const std::string& component_id, 
                                         long long timestamp, 
                                         const nlohmann::json& metrics) {
    try {
        // 检查必要字段
        if (!metrics.contains("cpu_percent") || !metrics.contains("memory_mb")) {
            return false;
        }
        
        // 插入组件指标
        SQLite::Statement insert(*db_, 
            "INSERT INTO component_metrics (component_id, timestamp, cpu_percent, memory_mb, gpu_percent) "
            "VALUES (?, ?, ?, ?, ?)");
        insert.bind(1, component_id);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, metrics["cpu_percent"].get<double>());
        insert.bind(4, metrics["memory_mb"].get<int>());
        insert.bind(5, metrics.contains("gpu_percent") ? metrics["gpu_percent"].get<double>() : 0.0);
        insert.exec();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Save component metrics error: " << e.what() << std::endl;
        return false;
    }
}

// 获取所有业务
nlohmann::json DatabaseManager::getBusinesses() {
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 查询所有业务
        SQLite::Statement query(*db_, 
            "SELECT business_id, business_name, status, created_at, updated_at FROM businesses");
        
        while (query.executeStep()) {
            nlohmann::json business;
            business["business_id"] = query.getColumn(0).getString();
            business["business_name"] = query.getColumn(1).getString();
            business["status"] = query.getColumn(2).getString();
            business["created_at"] = query.getColumn(3).getInt64();
            business["updated_at"] = query.getColumn(4).getInt64();
            
            result.push_back(business);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get businesses error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取业务详情
nlohmann::json DatabaseManager::getBusinessDetails(const std::string& business_id) {
    try {
        // 查询业务信息
        SQLite::Statement query(*db_, 
            "SELECT business_id, business_name, status, created_at, updated_at "
            "FROM businesses WHERE business_id = ?");
        query.bind(1, business_id);
        
        if (query.executeStep()) {
            nlohmann::json business;
            business["business_id"] = query.getColumn(0).getString();
            business["business_name"] = query.getColumn(1).getString();
            business["status"] = query.getColumn(2).getString();
            business["created_at"] = query.getColumn(3).getInt64();
            business["updated_at"] = query.getColumn(4).getInt64();
            
            // 查询业务组件
            business["components"] = getBusinessComponents(business_id);
            
            return business;
        }
        
        return nlohmann::json();
    } catch (const std::exception& e) {
        std::cerr << "Get business details error: " << e.what() << std::endl;
        return nlohmann::json();
    }
}

// 获取业务组件
nlohmann::json DatabaseManager::getBusinessComponents(const std::string& business_id) {
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 查询业务组件
        SQLite::Statement query(*db_, 
            "SELECT component_id, component_name, type, image_url, image_name, "
            "resource_requirements, environment_variables, config_files, affinity, "
            "node_id, container_id, status, started_at, updated_at "
            "FROM business_components WHERE business_id = ?");
        query.bind(1, business_id);
        
        while (query.executeStep()) {
            nlohmann::json component;
            component["component_id"] = query.getColumn(0).getString();
            component["component_name"] = query.getColumn(1).getString();
            component["type"] = query.getColumn(2).getString();
            component["image_url"] = query.getColumn(3).getString();
            component["image_name"] = query.getColumn(4).getString();
            
            // 解析JSON字段
            try {
                component["resource_requirements"] = nlohmann::json::parse(query.getColumn(5).getString());
                component["environment_variables"] = nlohmann::json::parse(query.getColumn(6).getString());
                component["config_files"] = nlohmann::json::parse(query.getColumn(7).getString());
                component["affinity"] = nlohmann::json::parse(query.getColumn(8).getString());
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON field: " << e.what() << std::endl;
            }
            
            component["node_id"] = query.getColumn(9).getString();
            component["container_id"] = query.getColumn(10).getString();
            component["status"] = query.getColumn(11).getString();
            component["started_at"] = query.getColumn(12).getInt64();
            component["updated_at"] = query.getColumn(13).getInt64();
            
            result.push_back(component);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get business components error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 获取组件详情
nlohmann::json DatabaseManager::getComponentDetails(const std::string& component_id) {
    try {
        // 查询组件信息
        SQLite::Statement query(*db_, 
            "SELECT component_id, business_id, component_name, type, image_url, image_name, "
            "resource_requirements, environment_variables, config_files, affinity, "
            "node_id, container_id, status, started_at, updated_at "
            "FROM business_components WHERE component_id = ?");
        query.bind(1, component_id);
        
        if (query.executeStep()) {
            nlohmann::json component;
            component["component_id"] = query.getColumn(0).getString();
            component["business_id"] = query.getColumn(1).getString();
            component["component_name"] = query.getColumn(2).getString();
            component["type"] = query.getColumn(3).getString();
            component["image_url"] = query.getColumn(4).getString();
            component["image_name"] = query.getColumn(5).getString();
            
            // 解析JSON字段
            try {
                component["resource_requirements"] = nlohmann::json::parse(query.getColumn(6).getString());
                component["environment_variables"] = nlohmann::json::parse(query.getColumn(7).getString());
                component["config_files"] = nlohmann::json::parse(query.getColumn(8).getString());
                component["affinity"] = nlohmann::json::parse(query.getColumn(9).getString());
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON field: " << e.what() << std::endl;
            }
            
            component["node_id"] = query.getColumn(10).getString();
            component["container_id"] = query.getColumn(11).getString();
            component["status"] = query.getColumn(12).getString();
            component["started_at"] = query.getColumn(13).getInt64();
            component["updated_at"] = query.getColumn(14).getInt64();
            
            return component;
        }
        
        return nlohmann::json();
    } catch (const std::exception& e) {
        std::cerr << "Get component details error: " << e.what() << std::endl;
        return nlohmann::json();
    }
}

// 获取组件指标
nlohmann::json DatabaseManager::getComponentMetrics(const std::string& component_id, int limit) {
    try {
        nlohmann::json result = nlohmann::json::array();
        
        // 查询组件指标
        SQLite::Statement query(*db_, 
            "SELECT timestamp, cpu_percent, memory_mb, gpu_percent "
            "FROM component_metrics WHERE component_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, component_id);
        query.bind(2, limit);
        
        while (query.executeStep()) {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["cpu_percent"] = query.getColumn(1).getDouble();
            metric["memory_mb"] = query.getColumn(2).getInt();
            metric["gpu_percent"] = query.getColumn(3).getDouble();
            
            result.push_back(metric);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get component metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

// 删除业务
bool DatabaseManager::deleteBusiness(const std::string& business_id) {
    try {
        // 开始事务
        SQLite::Transaction transaction(*db_);
        
        // 删除业务组件指标
        SQLite::Statement delete_metrics(*db_, 
            "DELETE FROM component_metrics WHERE component_id IN "
            "(SELECT component_id FROM business_components WHERE business_id = ?)");
        delete_metrics.bind(1, business_id);
        delete_metrics.exec();
        
        // 删除业务组件
        SQLite::Statement delete_components(*db_, 
            "DELETE FROM business_components WHERE business_id = ?");
        delete_components.bind(1, business_id);
        delete_components.exec();
        
        // 删除业务
        SQLite::Statement delete_business(*db_, 
            "DELETE FROM businesses WHERE business_id = ?");
        delete_business.bind(1, business_id);
        delete_business.exec();
        
        // 提交事务
        transaction.commit();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Delete business error: " << e.what() << std::endl;
        return false;
    }
}

// 获取节点资源信息
nlohmann::json DatabaseManager::getNodeResourceInfo(const std::string& node_id) {
    try {
        nlohmann::json result;
        
        // 获取最新的CPU指标
        {
            SQLite::Statement query(*db_, 
                "SELECT usage_percent, core_count "
                "FROM cpu_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            query.bind(1, node_id);
            
            if (query.executeStep()) {
                result["cpu_usage_percent"] = query.getColumn(0).getDouble();
                result["cpu_core_count"] = query.getColumn(1).getInt();
            }
        }
        
        // 获取最新的内存指标
        {
            SQLite::Statement query(*db_, 
                "SELECT total, used, free, usage_percent "
                "FROM memory_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            query.bind(1, node_id);
            
            if (query.executeStep()) {
                result["memory_total"] = query.getColumn(0).getInt64();
                result["memory_used"] = query.getColumn(1).getInt64();
                result["memory_free"] = query.getColumn(2).getInt64();
                result["memory_usage_percent"] = query.getColumn(3).getDouble();
            }
        }
        
        // 获取最新的Docker指标
        {
            SQLite::Statement query(*db_, 
                "SELECT id FROM docker_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            query.bind(1, node_id);
            
            if (query.executeStep()) {
                long long docker_metric_id = query.getColumn(0).getInt64();
                
                // 获取GPU使用情况（通过容器）
                SQLite::Statement container_query(*db_, 
                    "SELECT SUM(cpu_percent), SUM(memory_usage) "
                    "FROM docker_containers WHERE docker_metric_id = ?");
                container_query.bind(1, static_cast<int64_t>(docker_metric_id));
                
                if (container_query.executeStep()) {
                    result["docker_cpu_percent"] = container_query.getColumn(0).getDouble();
                    result["docker_memory_usage"] = container_query.getColumn(1).getInt64();
                }
            }
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Get node resource info error: " << e.what() << std::endl;
        return nlohmann::json();
    }
}