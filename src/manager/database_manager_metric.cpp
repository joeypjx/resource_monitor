#include "database_manager.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <map>
#include <vector>
#include <algorithm>

bool DatabaseManager::initializeMetricTables()
{
    try
    {
        // 创建cpu_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS cpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                usage_percent REAL NOT NULL,
                load_avg_1m REAL NOT NULL,
                load_avg_5m REAL NOT NULL,
                load_avg_15m REAL NOT NULL,
                core_count INTEGER NOT NULL,
                FOREIGN KEY (node_id) REFERENCES node(node_id)
            )
        )");

        // 创建memory_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS memory_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (node_id) REFERENCES node(node_id)
            )
        )");

        // 创建索引以提高查询性能
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_node_id ON cpu_metrics(node_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_timestamp ON cpu_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_node_id ON memory_metrics(node_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_timestamp ON memory_metrics(timestamp)");

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Metric tables initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveCpuMetrics(const std::string &node_id,
                                     long long timestamp,
                                     const nlohmann::json &cpu_data)
{
    try
    {
        // 检查必要字段
        if (!cpu_data.contains("usage_percent") || !cpu_data.contains("load_avg_1m") ||
            !cpu_data.contains("load_avg_5m") || !cpu_data.contains("load_avg_15m") ||
            !cpu_data.contains("core_count"))
        {
            return false;
        }

        // 插入CPU指标
        SQLite::Statement insert(*db_,
                                 "INSERT INTO cpu_metrics (node_id, timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, node_id);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, cpu_data["usage_percent"].get<double>());
        insert.bind(4, cpu_data["load_avg_1m"].get<double>());
        insert.bind(5, cpu_data["load_avg_5m"].get<double>());
        insert.bind(6, cpu_data["load_avg_15m"].get<double>());
        insert.bind(7, cpu_data["core_count"].get<int>());
        insert.exec();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save CPU metrics error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveMemoryMetrics(const std::string &node_id,
                                        long long timestamp,
                                        const nlohmann::json &memory_data)
{
    try
    {
        // 检查必要字段
        if (!memory_data.contains("total") || !memory_data.contains("used") ||
            !memory_data.contains("free") || !memory_data.contains("usage_percent"))
        {
            return false;
        }

        // 插入内存指标
        SQLite::Statement insert(*db_,
                                 "INSERT INTO memory_metrics (node_id, timestamp, total, used, free, usage_percent) "
                                 "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, node_id);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, static_cast<int64_t>(memory_data["total"].get<unsigned long long>()));
        insert.bind(4, static_cast<int64_t>(memory_data["used"].get<unsigned long long>()));
        insert.bind(5, static_cast<int64_t>(memory_data["free"].get<unsigned long long>()));
        insert.bind(6, memory_data["usage_percent"].get<double>());
        insert.exec();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save memory metrics error: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json DatabaseManager::getCpuMetrics(const std::string &node_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询CPU指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count "
                                "FROM cpu_metrics WHERE node_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, node_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get CPU metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getMemoryMetrics(const std::string &node_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询内存指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, total, used, free, usage_percent "
                                "FROM memory_metrics WHERE node_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, node_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["total"] = query.getColumn(1).getInt64();
            metric["used"] = query.getColumn(2).getInt64();
            metric["free"] = query.getColumn(3).getInt64();
            metric["usage_percent"] = query.getColumn(4).getDouble();

            result.push_back(metric);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get memory metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getNodeResourceHistory(const std::string& node_id, int limit)
{
    // cpu metrics
    auto cpu_metrics = getCpuMetrics(node_id, limit);
    // memory metrics
    auto memory_metrics = getMemoryMetrics(node_id, limit);

    nlohmann::json result;
    result["cpu_metrics"] = cpu_metrics;
    result["memory_metrics"] = memory_metrics;

    return result;
}

bool DatabaseManager::saveResourceUsage(const nlohmann::json &resource_usage)
{
    // 检查必要字段
    if (!resource_usage.contains("node_id") || !resource_usage.contains("timestamp") || !resource_usage.contains("resource")) {
        return false;
    }
    std::string node_id = resource_usage["node_id"];
    long long timestamp = resource_usage["timestamp"];
    const auto& resource = resource_usage["resource"];
    // 更新Board最后一次上报时间
    updateNodeLastSeen(node_id);
    // 保存各类资源数据
    if (resource.contains("cpu")) {
        saveCpuMetrics(node_id, timestamp, resource["cpu"]);
    }
    if (resource.contains("memory")) {
        saveMemoryMetrics(node_id, timestamp, resource["memory"]);
    }
    
    return true;
}