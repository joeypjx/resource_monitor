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
                board_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                usage_percent REAL NOT NULL,
                load_avg_1m REAL NOT NULL,
                load_avg_5m REAL NOT NULL,
                load_avg_15m REAL NOT NULL,
                core_count INTEGER NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id)
            )
        )");

        // 创建memory_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS memory_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id)
            )
        )");

        // 创建disk_metrics表（只保存汇总信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS disk_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                disk_count INTEGER NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id)
            )
        )");

        // 创建disk_usage表（保存每个磁盘详细信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS disk_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                disk_metrics_id INTEGER NOT NULL,
                device TEXT NOT NULL,
                mount_point TEXT NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (disk_metrics_id) REFERENCES disk_metrics(id)
            )
        )");

        // 创建network_metrics表（只保存汇总信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS network_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                network_count INTEGER NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id)
            )
        )");

        // 创建network_usage表（保存每个网卡详细信息）
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS network_usage (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                network_metrics_id INTEGER NOT NULL,
                interface TEXT NOT NULL,
                rx_bytes BIGINT NOT NULL,
                tx_bytes BIGINT NOT NULL,
                rx_packets BIGINT NOT NULL,
                tx_packets BIGINT NOT NULL,
                rx_errors INTEGER NOT NULL,
                tx_errors INTEGER NOT NULL,
                FOREIGN KEY (network_metrics_id) REFERENCES network_metrics(id)
            )
        )");

        // 创建docker_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS docker_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                board_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                container_count INTEGER NOT NULL,
                running_count INTEGER NOT NULL,
                paused_count INTEGER NOT NULL,
                stopped_count INTEGER NOT NULL,
                FOREIGN KEY (board_id) REFERENCES board(board_id)
            )
        )");

        // 创建docker_containers表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS docker_containers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                docker_metric_id INTEGER NOT NULL,
                container_id TEXT NOT NULL,
                name TEXT NOT NULL,
                image TEXT NOT NULL,
                status TEXT NOT NULL,
                cpu_percent REAL NOT NULL,
                memory_usage BIGINT NOT NULL,
                FOREIGN KEY (docker_metric_id) REFERENCES docker_metrics(id)
            )
        )");

        // 创建索引以提高查询性能
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_board_id ON cpu_metrics(board_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_timestamp ON cpu_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_board_id ON memory_metrics(board_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_timestamp ON memory_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_disk_metrics_board_id ON disk_metrics(board_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_disk_metrics_timestamp ON disk_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_network_metrics_board_id ON network_metrics(board_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_network_metrics_timestamp ON network_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_docker_metrics_board_id ON docker_metrics(board_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_docker_metrics_timestamp ON docker_metrics(timestamp)");

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Metric tables initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveCpuMetrics(const std::string &board_id,
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
                                 "INSERT INTO cpu_metrics (board_id, timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, board_id);
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

bool DatabaseManager::saveMemoryMetrics(const std::string &board_id,
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
                                 "INSERT INTO memory_metrics (board_id, timestamp, total, used, free, usage_percent) "
                                 "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, board_id);
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

bool DatabaseManager::saveDiskMetrics(const std::string &board_id,
                                      long long timestamp,
                                      const nlohmann::json &disk_data)
{
    try
    {
        if (!disk_data.is_array())
        {
            return false;
        }

        int disk_count = disk_data.size();

        // 1. 插入disk_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO disk_metrics (board_id, timestamp, disk_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, board_id);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, disk_count);
        insert_metrics.exec();

        long long disk_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个磁盘详细信息到disk_usage
        for (const auto &disk : disk_data)
        {
            if (!disk.contains("device") || !disk.contains("mount_point") ||
                !disk.contains("total") || !disk.contains("used") ||
                !disk.contains("free") || !disk.contains("usage_percent"))
            {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO disk_usage (disk_metrics_id, device, mount_point, total, used, free, usage_percent) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(disk_metrics_id));
            insert_usage.bind(2, disk["device"].get<std::string>());
            insert_usage.bind(3, disk["mount_point"].get<std::string>());
            insert_usage.bind(4, static_cast<int64_t>(disk["total"].get<unsigned long long>()));
            insert_usage.bind(5, static_cast<int64_t>(disk["used"].get<unsigned long long>()));
            insert_usage.bind(6, static_cast<int64_t>(disk["free"].get<unsigned long long>()));
            insert_usage.bind(7, disk["usage_percent"].get<double>());
            insert_usage.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save disk metrics error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveNetworkMetrics(const std::string &board_id,
                                         long long timestamp,
                                         const nlohmann::json &network_data)
{
    try
    {
        
        if (!network_data.is_array())
        {
            return false;
        }

        int network_count = network_data.size();

        // 1. 插入network_metrics汇总信息
        SQLite::Statement insert_metrics(*db_,
            "INSERT INTO network_metrics (board_id, timestamp, network_count) VALUES (?, ?, ?)");
        insert_metrics.bind(1, board_id);
        insert_metrics.bind(2, static_cast<int64_t>(timestamp));
        insert_metrics.bind(3, network_count);
        insert_metrics.exec();

        long long network_metrics_id = db_->getLastInsertRowid();

        // 2. 插入每个网卡详细信息到network_usage
        for (const auto &network : network_data)
        {
            if (!network.contains("interface") || !network.contains("rx_bytes") ||
                !network.contains("tx_bytes") || !network.contains("rx_packets") ||
                !network.contains("tx_packets") || !network.contains("rx_errors") ||
                !network.contains("tx_errors"))
            {
                continue;
            }

            SQLite::Statement insert_usage(*db_,
                "INSERT INTO network_usage (network_metrics_id, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert_usage.bind(1, static_cast<int64_t>(network_metrics_id));
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save network metrics error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveDockerMetrics(const std::string &board_id,
                                        long long timestamp,
                                        const nlohmann::json &docker_data)
{
    try
    {
        // 检查必要字段
        if (!docker_data.contains("container_count") || !docker_data.contains("running_count") ||
            !docker_data.contains("paused_count") || !docker_data.contains("stopped_count") ||
            !docker_data.contains("containers"))
        {
            return false;
        }

        // 插入Docker指标
        SQLite::Statement insert(*db_,
                                 "INSERT INTO docker_metrics (board_id, timestamp, container_count, running_count, paused_count, stopped_count) "
                                 "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, board_id);
        insert.bind(2, static_cast<int64_t>(timestamp));
        insert.bind(3, docker_data["container_count"].get<int>());
        insert.bind(4, docker_data["running_count"].get<int>());
        insert.bind(5, docker_data["paused_count"].get<int>());
        insert.bind(6, docker_data["stopped_count"].get<int>());
        insert.exec();

        // 获取插入的Docker指标ID
        long long docker_metric_id = db_->getLastInsertRowid();

        // 遍历所有容器
        for (const auto &container : docker_data["containers"])
        {
            // 检查必要字段
            if (!container.contains("id") || !container.contains("name") ||
                !container.contains("image") || !container.contains("status") ||
                !container.contains("cpu_percent") || !container.contains("memory_usage"))
            {
                continue;
            }

            // 插入容器信息
            SQLite::Statement insert_container(*db_,
                                               "INSERT INTO docker_containers (docker_metric_id, container_id, name, image, status, cpu_percent, memory_usage) "
                                               "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert_container.bind(1, static_cast<int64_t>(docker_metric_id));
            insert_container.bind(2, container["id"].get<std::string>());
            insert_container.bind(3, container["name"].get<std::string>());
            insert_container.bind(4, container["image"].get<std::string>());
            insert_container.bind(5, container["status"].get<std::string>());
            insert_container.bind(6, container["cpu_percent"].get<double>());
            insert_container.bind(7, static_cast<int64_t>(container["memory_usage"].get<unsigned long long>()));
            insert_container.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save Docker metrics error: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json DatabaseManager::getCpuMetrics(const std::string &board_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询CPU指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count "
                                "FROM cpu_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, board_id);
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

nlohmann::json DatabaseManager::getMemoryMetrics(const std::string &board_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询内存指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, total, used, free, usage_percent "
                                "FROM memory_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, board_id);
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

nlohmann::json DatabaseManager::getDiskMetrics(const std::string &board_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询disk_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, disk_count FROM disk_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, board_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            long long disk_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["disk_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有磁盘详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT device, mount_point, total, used, free, usage_percent FROM disk_usage WHERE disk_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(disk_metrics_id));

            nlohmann::json disks = nlohmann::json::array();
            while (usage_query.executeStep())
            {
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get disk metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getNetworkMetrics(const std::string &board_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询network_metrics
        SQLite::Statement query(*db_,
            "SELECT id, timestamp, network_count FROM network_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, board_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            long long network_metrics_id = query.getColumn(0).getInt64();
            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["network_count"] = query.getColumn(2).getInt();

            // 查询该时间点所有网卡详细信息
            SQLite::Statement usage_query(*db_,
                "SELECT interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors FROM network_usage WHERE network_metrics_id = ?");
            usage_query.bind(1, static_cast<int64_t>(network_metrics_id));

            nlohmann::json networks = nlohmann::json::array();
            while (usage_query.executeStep())
            {
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get network metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getDockerMetrics(const std::string &board_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询Docker指标
        SQLite::Statement query(*db_,
                                "SELECT id, timestamp, container_count, running_count, paused_count, stopped_count "
                                "FROM docker_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, board_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            long long docker_metric_id = query.getColumn(0).getInt64();

            metric["timestamp"] = query.getColumn(1).getInt64();
            metric["container_count"] = query.getColumn(2).getInt();
            metric["running_count"] = query.getColumn(3).getInt();
            metric["paused_count"] = query.getColumn(4).getInt();
            metric["stopped_count"] = query.getColumn(5).getInt();

            // 查询容器信息
            SQLite::Statement container_query(*db_,
                                              "SELECT container_id, name, image, status, cpu_percent, memory_usage "
                                              "FROM docker_containers WHERE docker_metric_id = ?");
            container_query.bind(1, static_cast<int64_t>(docker_metric_id));

            nlohmann::json containers = nlohmann::json::array();

            while (container_query.executeStep())
            {
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
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get Docker metrics error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getNodeResourceHistory(const std::string& node_id, int limit)
{
    // cpu metrics
    auto cpu_metrics = getCpuMetrics(node_id, limit);
    // memory metrics
    auto memory_metrics = getMemoryMetrics(node_id, limit);
    // disk metrics
    auto disk_metrics = getDiskMetrics(node_id, limit);
    // network metrics
    auto network_metrics = getNetworkMetrics(node_id, limit);
    // docker metrics
    auto docker_metrics = getDockerMetrics(node_id, limit);

    nlohmann::json result;
    result["cpu_metrics"] = cpu_metrics;
    result["memory_metrics"] = memory_metrics;
    result["disk_metrics"] = disk_metrics;
    result["network_metrics"] = network_metrics;
    result["docker_metrics"] = docker_metrics;

    return result;
}

bool DatabaseManager::saveResourceUsage(const nlohmann::json &resource_usage)
{
    // 检查必要字段
    if (!resource_usage.contains("board_id") || !resource_usage.contains("timestamp") || !resource_usage.contains("resource")) {
        return false;
    }
    std::string board_id = resource_usage["board_id"];
    long long timestamp = resource_usage["timestamp"];
    const auto& resource = resource_usage["resource"];
    // 更新Board最后一次上报时间
    updateBoardLastSeen(board_id);
    // 保存各类资源数据
    if (resource.contains("cpu")) {
        saveCpuMetrics(board_id, timestamp, resource["cpu"]);
    }
    if (resource.contains("memory")) {
        saveMemoryMetrics(board_id, timestamp, resource["memory"]);
    }
    if (resource.contains("disk")) {
        saveDiskMetrics(board_id, timestamp, resource["disk"]);
    }
    if (resource.contains("network")) {
        saveNetworkMetrics(board_id, timestamp, resource["network"]);
    }
    if (resource.contains("docker")) {
        saveDockerMetrics(board_id, timestamp, resource["docker"]);
    }
    return true;
}

// 集群资源聚合
nlohmann::json DatabaseManager::getClusterMetrics() {
    nlohmann::json result;
    try {
        // 1. 节点统计
        int total_nodes = 0, online_nodes = 0, offline_nodes = 0;
        SQLite::Statement board_query(*db_, "SELECT board_id, status FROM board");
        std::vector<std::string> board_ids;
        while (board_query.executeStep()) {
            total_nodes++;
            std::string status = board_query.getColumn(1).getString();
            if (status == "online") online_nodes++;
            else offline_nodes++;
            board_ids.push_back(board_query.getColumn(0).getString());
        }
        result["total_nodes"] = total_nodes;
        result["online_nodes"] = online_nodes;
        result["offline_nodes"] = offline_nodes;

        // 2. 资源聚合
        int total_cores = 0;
        double used_cores = 0.0, cpu_usage_sum = 0.0, load_avg_1m_sum = 0.0, load_avg_5m_sum = 0.0, load_avg_15m_sum = 0.0;
        int cpu_count = 0;
        int64_t mem_total = 0, mem_used = 0, mem_free = 0;
        double mem_usage_sum = 0.0;
        int mem_count = 0;
        int64_t disk_total = 0, disk_used = 0, disk_free = 0;
        double disk_usage_sum = 0.0;
        int disk_count = 0;
        int64_t net_rx_bytes = 0, net_tx_bytes = 0, net_rx_packets = 0, net_tx_packets = 0;
        int net_count = 0;
        int container_count = 0, running_count = 0, paused_count = 0, stopped_count = 0;
        int docker_count = 0;

        for (const auto& board_id : board_ids) {
            // CPU
            SQLite::Statement cpu_query(*db_, "SELECT usage_percent, core_count, load_avg_1m, load_avg_5m, load_avg_15m FROM cpu_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            cpu_query.bind(1, board_id);
            if (cpu_query.executeStep()) {
                double usage_percent = cpu_query.getColumn(0).getDouble();
                int core_count = cpu_query.getColumn(1).getInt();
                double load1 = cpu_query.getColumn(2).getDouble();
                double load5 = cpu_query.getColumn(3).getDouble();
                double load15 = cpu_query.getColumn(4).getDouble();
                total_cores += core_count;
                used_cores += core_count * usage_percent / 100.0;
                cpu_usage_sum += usage_percent;
                load_avg_1m_sum += load1;
                load_avg_5m_sum += load5;
                load_avg_15m_sum += load15;
                cpu_count++;
            }
            // Memory
            SQLite::Statement mem_query(*db_, "SELECT total, used, free, usage_percent FROM memory_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            mem_query.bind(1, board_id);
            if (mem_query.executeStep()) {
                int64_t t = mem_query.getColumn(0).getInt64();
                int64_t u = mem_query.getColumn(1).getInt64();
                int64_t f = mem_query.getColumn(2).getInt64();
                double usage = mem_query.getColumn(3).getDouble();
                mem_total += t;
                mem_used += u;
                mem_free += f;
                mem_usage_sum += usage;
                mem_count++;
            }
            // Disk
            SQLite::Statement disk_metrics_query(*db_, "SELECT id FROM disk_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            disk_metrics_query.bind(1, board_id);
            if (disk_metrics_query.executeStep()) {
                long long disk_metrics_id = disk_metrics_query.getColumn(0).getInt64();
                SQLite::Statement disk_usage_query(*db_, "SELECT total, used, free, usage_percent FROM disk_usage WHERE disk_metrics_id = ?");
                disk_usage_query.bind(1, static_cast<int64_t>(disk_metrics_id));
                while (disk_usage_query.executeStep()) {
                    disk_total += disk_usage_query.getColumn(0).getInt64();
                    disk_used += disk_usage_query.getColumn(1).getInt64();
                    disk_free += disk_usage_query.getColumn(2).getInt64();
                    disk_usage_sum += disk_usage_query.getColumn(3).getDouble();
                    disk_count++;
                }
            }
            // Network
            SQLite::Statement net_metrics_query(*db_, "SELECT id FROM network_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            net_metrics_query.bind(1, board_id);
            if (net_metrics_query.executeStep()) {
                long long net_metrics_id = net_metrics_query.getColumn(0).getInt64();
                SQLite::Statement net_usage_query(*db_, "SELECT rx_bytes, tx_bytes, rx_packets, tx_packets FROM network_usage WHERE network_metrics_id = ?");
                net_usage_query.bind(1, static_cast<int64_t>(net_metrics_id));
                while (net_usage_query.executeStep()) {
                    net_rx_bytes += net_usage_query.getColumn(0).getInt64();
                    net_tx_bytes += net_usage_query.getColumn(1).getInt64();
                    net_rx_packets += net_usage_query.getColumn(2).getInt64();
                    net_tx_packets += net_usage_query.getColumn(3).getInt64();
                    net_count++;
                }
            }
            // Docker
            SQLite::Statement docker_query(*db_, "SELECT container_count, running_count, paused_count, stopped_count FROM docker_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT 1");
            docker_query.bind(1, board_id);
            if (docker_query.executeStep()) {
                container_count += docker_query.getColumn(0).getInt();
                running_count += docker_query.getColumn(1).getInt();
                paused_count += docker_query.getColumn(2).getInt();
                stopped_count += docker_query.getColumn(3).getInt();
                docker_count++;
            }
        }
        // 3. 汇总
        nlohmann::json cpu_json;
        cpu_json["total_cores"] = total_cores;
        cpu_json["used_cores"] = used_cores;
        cpu_json["usage_percent"] = cpu_count > 0 ? cpu_usage_sum / cpu_count : 0.0;
        cpu_json["load_avg_1m"] = cpu_count > 0 ? load_avg_1m_sum / cpu_count : 0.0;
        cpu_json["load_avg_5m"] = cpu_count > 0 ? load_avg_5m_sum / cpu_count : 0.0;
        cpu_json["load_avg_15m"] = cpu_count > 0 ? load_avg_15m_sum / cpu_count : 0.0;
        result["cpu"] = cpu_json;
        nlohmann::json mem_json;
        mem_json["total"] = mem_total;
        mem_json["used"] = mem_used;
        mem_json["free"] = mem_free;
        mem_json["usage_percent"] = mem_total > 0 ? (double)mem_used / mem_total * 100.0 : 0.0;
        result["memory"] = mem_json;
        nlohmann::json disk_json;
        disk_json["total"] = disk_total;
        disk_json["used"] = disk_used;
        disk_json["free"] = disk_free;
        disk_json["usage_percent"] = disk_total > 0 ? (double)disk_used / disk_total * 100.0 : 0.0;
        result["disk"] = disk_json;
        nlohmann::json net_json;
        net_json["total_rx_bytes"] = net_rx_bytes;
        net_json["total_tx_bytes"] = net_tx_bytes;
        net_json["total_rx_packets"] = net_rx_packets;
        net_json["total_tx_packets"] = net_tx_packets;
        result["network"] = net_json;
        nlohmann::json docker_json;
        docker_json["container_count"] = container_count;
        docker_json["running_count"] = running_count;
        docker_json["paused_count"] = paused_count;
        docker_json["stopped_count"] = stopped_count;
        result["docker"] = docker_json;
        result["timestamp"] = std::time(nullptr);
        result["status"] = "success";
    } catch (const std::exception& e) {
        result["status"] = "error";
        result["message"] = e.what();
    }
    return result;
}

nlohmann::json DatabaseManager::getClusterMetricsHistory(int limit) {
    nlohmann::json result;
    try {
        // 1. 获取所有节点ID
        std::vector<std::string> board_ids;
        SQLite::Statement board_query(*db_, "SELECT board_id FROM board");
        while (board_query.executeStep()) {
            board_ids.push_back(board_query.getColumn(0).getString());
        }
        if (board_ids.empty()) {
            result["status"] = "success";
            result["history"] = nlohmann::json::array();
            return result;
        }
        // 2. 获取每个节点最新limit条CPU、内存、磁盘、网络、Docker数据，按时间戳聚合
        // 以CPU为主（因为所有节点都会上报CPU）
        // 收集所有节点的时间戳
        std::map<long long, std::vector<std::string>> ts_to_boards;
        std::map<std::string, std::vector<long long>> board_to_ts;
        for (const auto& board_id : board_ids) {
            SQLite::Statement cpu_query(*db_, "SELECT timestamp FROM cpu_metrics WHERE board_id = ? ORDER BY timestamp DESC LIMIT ?");
            cpu_query.bind(1, board_id);
            cpu_query.bind(2, limit);
            while (cpu_query.executeStep()) {
                long long ts = cpu_query.getColumn(0).getInt64();
                ts_to_boards[ts].push_back(board_id);
                board_to_ts[board_id].push_back(ts);
            }
        }
        // 取最新limit个时间点
        std::vector<long long> all_ts;
        for (const auto& kv : ts_to_boards) all_ts.push_back(kv.first);
        std::sort(all_ts.begin(), all_ts.end(), std::greater<long long>());
        if ((int)all_ts.size() > limit) all_ts.resize(limit);
        // 3. 对每个时间点聚合所有节点的资源
        nlohmann::json history = nlohmann::json::array();
        for (auto ts : all_ts) {
            int total_cores = 0;
            double used_cores = 0.0, cpu_usage_sum = 0.0, load_avg_1m_sum = 0.0, load_avg_5m_sum = 0.0, load_avg_15m_sum = 0.0;
            int cpu_count = 0;
            int64_t mem_total = 0, mem_used = 0, mem_free = 0;
            double mem_usage_sum = 0.0;
            int mem_count = 0;
            int64_t disk_total = 0, disk_used = 0, disk_free = 0;
            double disk_usage_sum = 0.0;
            int disk_count = 0;
            int64_t net_rx_bytes = 0, net_tx_bytes = 0, net_rx_packets = 0, net_tx_packets = 0;
            int net_count = 0;
            int container_count = 0, running_count = 0, paused_count = 0, stopped_count = 0;
            int docker_count = 0;
            for (const auto& board_id : board_ids) {
                // CPU
                SQLite::Statement cpu_query(*db_, "SELECT usage_percent, core_count, load_avg_1m, load_avg_5m, load_avg_15m FROM cpu_metrics WHERE board_id = ? AND timestamp = ?");
                cpu_query.bind(1, board_id);
                cpu_query.bind(2, static_cast<int64_t>(ts));
                if (cpu_query.executeStep()) {
                    double usage_percent = cpu_query.getColumn(0).getDouble();
                    int core_count = cpu_query.getColumn(1).getInt();
                    double load1 = cpu_query.getColumn(2).getDouble();
                    double load5 = cpu_query.getColumn(3).getDouble();
                    double load15 = cpu_query.getColumn(4).getDouble();
                    total_cores += core_count;
                    used_cores += core_count * usage_percent / 100.0;
                    cpu_usage_sum += usage_percent;
                    load_avg_1m_sum += load1;
                    load_avg_5m_sum += load5;
                    load_avg_15m_sum += load15;
                    cpu_count++;
                }
                // Memory
                SQLite::Statement mem_query(*db_, "SELECT total, used, free, usage_percent FROM memory_metrics WHERE board_id = ? AND timestamp = ?");
                mem_query.bind(1, board_id);
                mem_query.bind(2, static_cast<int64_t>(ts));
                if (mem_query.executeStep()) {
                    int64_t t = mem_query.getColumn(0).getInt64();
                    int64_t u = mem_query.getColumn(1).getInt64();
                    int64_t f = mem_query.getColumn(2).getInt64();
                    double usage = mem_query.getColumn(3).getDouble();
                    mem_total += t;
                    mem_used += u;
                    mem_free += f;
                    mem_usage_sum += usage;
                    mem_count++;
                }
                // Disk
                SQLite::Statement disk_metrics_query(*db_, "SELECT id FROM disk_metrics WHERE board_id = ? AND timestamp = ?");
                disk_metrics_query.bind(1, board_id);
                disk_metrics_query.bind(2, static_cast<int64_t>(ts));
                if (disk_metrics_query.executeStep()) {
                    long long disk_metrics_id = disk_metrics_query.getColumn(0).getInt64();
                    SQLite::Statement disk_usage_query(*db_, "SELECT total, used, free, usage_percent FROM disk_usage WHERE disk_metrics_id = ?");
                    disk_usage_query.bind(1, static_cast<int64_t>(disk_metrics_id));
                    while (disk_usage_query.executeStep()) {
                        disk_total += disk_usage_query.getColumn(0).getInt64();
                        disk_used += disk_usage_query.getColumn(1).getInt64();
                        disk_free += disk_usage_query.getColumn(2).getInt64();
                        disk_usage_sum += disk_usage_query.getColumn(3).getDouble();
                        disk_count++;
                    }
                }
                // Network
                SQLite::Statement net_metrics_query(*db_, "SELECT id FROM network_metrics WHERE board_id = ? AND timestamp = ?");
                net_metrics_query.bind(1, board_id);
                net_metrics_query.bind(2, static_cast<int64_t>(ts));
                if (net_metrics_query.executeStep()) {
                    long long net_metrics_id = net_metrics_query.getColumn(0).getInt64();
                    SQLite::Statement net_usage_query(*db_, "SELECT rx_bytes, tx_bytes, rx_packets, tx_packets FROM network_usage WHERE network_metrics_id = ?");
                    net_usage_query.bind(1, static_cast<int64_t>(net_metrics_id));
                    while (net_usage_query.executeStep()) {
                        net_rx_bytes += net_usage_query.getColumn(0).getInt64();
                        net_tx_bytes += net_usage_query.getColumn(1).getInt64();
                        net_rx_packets += net_usage_query.getColumn(2).getInt64();
                        net_tx_packets += net_usage_query.getColumn(3).getInt64();
                        net_count++;
                    }
                }
                // Docker
                SQLite::Statement docker_query(*db_, "SELECT container_count, running_count, paused_count, stopped_count FROM docker_metrics WHERE board_id = ? AND timestamp = ?");
                docker_query.bind(1, board_id);
                docker_query.bind(2, static_cast<int64_t>(ts));
                if (docker_query.executeStep()) {
                    container_count += docker_query.getColumn(0).getInt();
                    running_count += docker_query.getColumn(1).getInt();
                    paused_count += docker_query.getColumn(2).getInt();
                    stopped_count += docker_query.getColumn(3).getInt();
                    docker_count++;
                }
            }
            nlohmann::json item;
            item["timestamp"] = ts;
            nlohmann::json cpu_json;
            cpu_json["total_cores"] = total_cores;
            cpu_json["used_cores"] = used_cores;
            cpu_json["usage_percent"] = cpu_count > 0 ? cpu_usage_sum / cpu_count : 0.0;
            cpu_json["load_avg_1m"] = cpu_count > 0 ? load_avg_1m_sum / cpu_count : 0.0;
            cpu_json["load_avg_5m"] = cpu_count > 0 ? load_avg_5m_sum / cpu_count : 0.0;
            cpu_json["load_avg_15m"] = cpu_count > 0 ? load_avg_15m_sum / cpu_count : 0.0;
            item["cpu"] = cpu_json;
            nlohmann::json mem_json;
            mem_json["total"] = mem_total;
            mem_json["used"] = mem_used;
            mem_json["free"] = mem_free;
            mem_json["usage_percent"] = mem_total > 0 ? (double)mem_used / mem_total * 100.0 : 0.0;
            item["memory"] = mem_json;
            nlohmann::json disk_json;
            disk_json["total"] = disk_total;
            disk_json["used"] = disk_used;
            disk_json["free"] = disk_free;
            disk_json["usage_percent"] = disk_total > 0 ? (double)disk_used / disk_total * 100.0 : 0.0;
            item["disk"] = disk_json;
            nlohmann::json net_json;
            net_json["total_rx_bytes"] = net_rx_bytes;
            net_json["total_tx_bytes"] = net_tx_bytes;
            net_json["total_rx_packets"] = net_rx_packets;
            net_json["total_tx_packets"] = net_tx_packets;
            item["network"] = net_json;
            nlohmann::json docker_json;
            docker_json["container_count"] = container_count;
            docker_json["running_count"] = running_count;
            docker_json["paused_count"] = paused_count;
            docker_json["stopped_count"] = stopped_count;
            item["docker"] = docker_json;
            history.push_back(item);
        }
        result["status"] = "success";
        result["history"] = history;
    } catch (const std::exception& e) {
        result["status"] = "error";
        result["message"] = e.what();
    }
    return result;
} 