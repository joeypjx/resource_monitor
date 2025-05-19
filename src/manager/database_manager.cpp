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

        // 创建agents表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS agents (
                agent_id TEXT PRIMARY KEY,
                hostname TEXT NOT NULL,
                ip_address TEXT NOT NULL,
                os_info TEXT NOT NULL,
                first_seen TIMESTAMP NOT NULL,
                last_seen TIMESTAMP NOT NULL,
                status TEXT NOT NULL DEFAULT 'online'
            )
        )");

        // 创建cpu_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS cpu_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                usage_percent REAL NOT NULL,
                load_avg_1m REAL NOT NULL,
                load_avg_5m REAL NOT NULL,
                load_avg_15m REAL NOT NULL,
                core_count INTEGER NOT NULL,
                FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
            )
        )");

        // 创建memory_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS memory_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
            )
        )");

        // 创建disk_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS disk_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                device TEXT NOT NULL,
                mount_point TEXT NOT NULL,
                total BIGINT NOT NULL,
                used BIGINT NOT NULL,
                free BIGINT NOT NULL,
                usage_percent REAL NOT NULL,
                FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
            )
        )");

        // 创建network_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS network_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                interface TEXT NOT NULL,
                rx_bytes BIGINT NOT NULL,
                tx_bytes BIGINT NOT NULL,
                rx_packets BIGINT NOT NULL,
                tx_packets BIGINT NOT NULL,
                rx_errors INTEGER NOT NULL,
                tx_errors INTEGER NOT NULL,
                FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
            )
        )");

        // 创建docker_metrics表
        db_->exec(R"(
            CREATE TABLE IF NOT EXISTS docker_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                agent_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                container_count INTEGER NOT NULL,
                running_count INTEGER NOT NULL,
                paused_count INTEGER NOT NULL,
                stopped_count INTEGER NOT NULL,
                FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
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
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_agent_id ON cpu_metrics(agent_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_cpu_metrics_timestamp ON cpu_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_agent_id ON memory_metrics(agent_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_memory_metrics_timestamp ON memory_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_disk_metrics_agent_id ON disk_metrics(agent_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_disk_metrics_timestamp ON disk_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_network_metrics_agent_id ON network_metrics(agent_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_network_metrics_timestamp ON network_metrics(timestamp)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_docker_metrics_agent_id ON docker_metrics(agent_id)");
        db_->exec("CREATE INDEX IF NOT EXISTS idx_docker_metrics_timestamp ON docker_metrics(timestamp)");

        // 初始化业务相关的数据库表
        if (!initializeBusinessTables())
        {
            std::cerr << "Business tables initialization error" << std::endl;
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

bool DatabaseManager::saveAgent(const nlohmann::json &agent_info)
{
    try
    {
        // 检查必要字段
        if (!agent_info.contains("agent_id") || !agent_info.contains("hostname") ||
            !agent_info.contains("ip_address") || !agent_info.contains("os_info"))
        {
            return false;
        }

        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        // 检查Agent是否已存在
        SQLite::Statement query(*db_, "SELECT agent_id FROM agents WHERE agent_id = ?");
        query.bind(1, agent_info["agent_id"].get<std::string>());

        if (query.executeStep())
        {
            // Agent已存在，更新信息
            SQLite::Statement update(*db_,
                                     "UPDATE agents SET hostname = ?, ip_address = ?, os_info = ?, last_seen = ? WHERE agent_id = ?");
            update.bind(1, agent_info["hostname"].get<std::string>());
            update.bind(2, agent_info["ip_address"].get<std::string>());
            update.bind(3, agent_info["os_info"].get<std::string>());
            update.bind(4, static_cast<int64_t>(timestamp));
            update.bind(5, agent_info["agent_id"].get<std::string>());
            update.exec();
        }
        else
        {
            // 新Agent，插入记录
            SQLite::Statement insert(*db_,
                                     "INSERT INTO agents (agent_id, hostname, ip_address, os_info, first_seen, last_seen) VALUES (?, ?, ?, ?, ?, ?)");
            insert.bind(1, agent_info["agent_id"].get<std::string>());
            insert.bind(2, agent_info["hostname"].get<std::string>());
            insert.bind(3, agent_info["ip_address"].get<std::string>());
            insert.bind(4, agent_info["os_info"].get<std::string>());
            insert.bind(5, static_cast<int64_t>(timestamp));
            insert.bind(6, static_cast<int64_t>(timestamp));
            insert.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save agent error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateAgentLastSeen(const std::string &agent_id)
{
    try
    {
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        // 更新Agent最后活动时间和状态为在线
        SQLite::Statement update(*db_, "UPDATE agents SET last_seen = ?, status = 'online' WHERE agent_id = ?");
        update.bind(1, static_cast<int64_t>(timestamp));
        update.bind(2, agent_id);
        update.exec();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update agent last seen error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::updateAgentStatus(const std::string &agent_id, const std::string &status)
{
    try
    {
        // 更新Agent状态
        SQLite::Statement update(*db_, "UPDATE agents SET status = ? WHERE agent_id = ?");
        update.bind(1, status);
        update.bind(2, agent_id);
        update.exec();

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Update agent status error: " << e.what() << std::endl;
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
                SQLite::Statement query(*db_, "SELECT agent_id, last_seen FROM agents");
                
                while (query.executeStep()) {
                    std::string agent_id = query.getColumn(0).getString();
                    int64_t last_seen = query.getColumn(1).getInt64();
                    
                    // 如果超过5秒没有上报，则标记为离线
                    if (current_timestamp - last_seen > 5) {
                        updateAgentStatus(agent_id, "offline");
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

bool DatabaseManager::saveCpuMetrics(const std::string &agent_id,
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
                                 "INSERT INTO cpu_metrics (agent_id, timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, agent_id);
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

bool DatabaseManager::saveMemoryMetrics(const std::string &agent_id,
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
                                 "INSERT INTO memory_metrics (agent_id, timestamp, total, used, free, usage_percent) "
                                 "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, agent_id);
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

bool DatabaseManager::saveDiskMetrics(const std::string &agent_id,
                                      long long timestamp,
                                      const nlohmann::json &disk_data)
{
    try
    {
        // 检查是否为数组
        if (!disk_data.is_array())
        {
            return false;
        }

        // 遍历所有磁盘分区
        for (const auto &disk : disk_data)
        {
            // 检查必要字段
            if (!disk.contains("device") || !disk.contains("mount_point") ||
                !disk.contains("total") || !disk.contains("used") ||
                !disk.contains("free") || !disk.contains("usage_percent"))
            {
                continue;
            }

            // 插入磁盘指标
            SQLite::Statement insert(*db_,
                                     "INSERT INTO disk_metrics (agent_id, timestamp, device, mount_point, total, used, free, usage_percent) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            insert.bind(1, agent_id);
            insert.bind(2, static_cast<int64_t>(timestamp));
            insert.bind(3, disk["device"].get<std::string>());
            insert.bind(4, disk["mount_point"].get<std::string>());
            insert.bind(5, static_cast<int64_t>(disk["total"].get<unsigned long long>()));
            insert.bind(6, static_cast<int64_t>(disk["used"].get<unsigned long long>()));
            insert.bind(7, static_cast<int64_t>(disk["free"].get<unsigned long long>()));
            insert.bind(8, disk["usage_percent"].get<double>());
            insert.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save disk metrics error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveNetworkMetrics(const std::string &agent_id,
                                         long long timestamp,
                                         const nlohmann::json &network_data)
{
    try
    {
        // 检查是否为数组
        if (!network_data.is_array())
        {
            return false;
        }

        // 遍历所有网络接口
        for (const auto &network : network_data)
        {
            // 检查必要字段
            if (!network.contains("interface") || !network.contains("rx_bytes") ||
                !network.contains("tx_bytes") || !network.contains("rx_packets") ||
                !network.contains("tx_packets") || !network.contains("rx_errors") ||
                !network.contains("tx_errors"))
            {
                continue;
            }

            // 插入网络指标
            SQLite::Statement insert(*db_,
                                     "INSERT INTO network_metrics (agent_id, timestamp, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
            insert.bind(1, agent_id);
            insert.bind(2, static_cast<int64_t>(timestamp));
            insert.bind(3, network["interface"].get<std::string>());
            insert.bind(4, static_cast<int64_t>(network["rx_bytes"].get<unsigned long long>()));
            insert.bind(5, static_cast<int64_t>(network["tx_bytes"].get<unsigned long long>()));
            insert.bind(6, static_cast<int64_t>(network["rx_packets"].get<unsigned long long>()));
            insert.bind(7, static_cast<int64_t>(network["tx_packets"].get<unsigned long long>()));
            insert.bind(8, network["rx_errors"].get<int>());
            insert.bind(9, network["tx_errors"].get<int>());
            insert.exec();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Save network metrics error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::saveDockerMetrics(const std::string &agent_id,
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
                                 "INSERT INTO docker_metrics (agent_id, timestamp, container_count, running_count, paused_count, stopped_count) "
                                 "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, agent_id);
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

nlohmann::json DatabaseManager::getAgents()
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询所有Agent
        SQLite::Statement query(*db_, "SELECT agent_id, hostname, ip_address, os_info, first_seen, last_seen, status FROM agents");

        while (query.executeStep())
        {
            nlohmann::json agent;
            agent["agent_id"] = query.getColumn(0).getString();
            agent["hostname"] = query.getColumn(1).getString();
            agent["ip_address"] = query.getColumn(2).getString();
            agent["os_info"] = query.getColumn(3).getString();
            agent["first_seen"] = query.getColumn(4).getInt64();
            agent["last_seen"] = query.getColumn(5).getInt64();
            agent["status"] = query.getColumn(6).getString();

            result.push_back(agent);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Get agents error: " << e.what() << std::endl;
        return nlohmann::json::array();
    }
}

nlohmann::json DatabaseManager::getCpuMetrics(const std::string &agent_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询CPU指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count "
                                "FROM cpu_metrics WHERE agent_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, agent_id);
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

nlohmann::json DatabaseManager::getMemoryMetrics(const std::string &agent_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询内存指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, total, used, free, usage_percent "
                                "FROM memory_metrics WHERE agent_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, agent_id);
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

nlohmann::json DatabaseManager::getDiskMetrics(const std::string &agent_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询磁盘指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, device, mount_point, total, used, free, usage_percent "
                                "FROM disk_metrics WHERE agent_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, agent_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["device"] = query.getColumn(1).getString();
            metric["mount_point"] = query.getColumn(2).getString();
            metric["total"] = query.getColumn(3).getInt64();
            metric["used"] = query.getColumn(4).getInt64();
            metric["free"] = query.getColumn(5).getInt64();
            metric["usage_percent"] = query.getColumn(6).getDouble();

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

nlohmann::json DatabaseManager::getNetworkMetrics(const std::string &agent_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询网络指标
        SQLite::Statement query(*db_,
                                "SELECT timestamp, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors "
                                "FROM network_metrics WHERE agent_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, agent_id);
        query.bind(2, limit);

        while (query.executeStep())
        {
            nlohmann::json metric;
            metric["timestamp"] = query.getColumn(0).getInt64();
            metric["interface"] = query.getColumn(1).getString();
            metric["rx_bytes"] = query.getColumn(2).getInt64();
            metric["tx_bytes"] = query.getColumn(3).getInt64();
            metric["rx_packets"] = query.getColumn(4).getInt64();
            metric["tx_packets"] = query.getColumn(5).getInt64();
            metric["rx_errors"] = query.getColumn(6).getInt();
            metric["tx_errors"] = query.getColumn(7).getInt();

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

nlohmann::json DatabaseManager::getDockerMetrics(const std::string &agent_id, int limit)
{
    try
    {
        nlohmann::json result = nlohmann::json::array();

        // 查询Docker指标
        SQLite::Statement query(*db_,
                                "SELECT id, timestamp, container_count, running_count, paused_count, stopped_count "
                                "FROM docker_metrics WHERE agent_id = ? ORDER BY timestamp DESC LIMIT ?");
        query.bind(1, agent_id);
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


nlohmann::json DatabaseManager::getNode(const std::string &node_id)
{
    try
    {
        SQLite::Statement query(*db_, "SELECT * FROM agents WHERE agent_id = ?");
        query.bind(1, node_id);

        while (query.executeStep())
        {
            nlohmann::json node;
            node["agent_id"] = query.getColumn(0).getString();
            node["hostname"] = query.getColumn(1).getString();
            node["ip_address"] = query.getColumn(2).getString();
            node["os_info"] = query.getColumn(3).getString();
            node["first_seen"] = query.getColumn(4).getInt64();
            node["last_seen"] = query.getColumn(5).getInt64();
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

bool DatabaseManager::saveResourceUsage(const nlohmann::json &resource_usage)
{
        // 检查必要字段
    if (!resource_usage.contains("agent_id") || !resource_usage.contains("timestamp")) {
        return true;
    }
    
    std::string agent_id = resource_usage["agent_id"];
    long long timestamp = resource_usage["timestamp"];
    
    // 更新Agent最后一次上报时间
    updateAgentLastSeen(agent_id);
    
    // 保存各类资源数据
    if (resource_usage.contains("cpu")) {
        saveCpuMetrics(agent_id, timestamp, resource_usage["cpu"]);
    }
    
    if (resource_usage.contains("memory")) {
        saveMemoryMetrics(agent_id, timestamp, resource_usage["memory"]);
    }
    
    if (resource_usage.contains("disk")) {
        saveDiskMetrics(agent_id, timestamp, resource_usage["disk"]);
    }
    
    if (resource_usage.contains("network")) {
        saveNetworkMetrics(agent_id, timestamp, resource_usage["network"]);
    }
    
    if (resource_usage.contains("docker")) {
        saveDockerMetrics(agent_id, timestamp, resource_usage["docker"]);
    }
    
    return true;
}