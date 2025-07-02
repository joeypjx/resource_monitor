#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
#include <unordered_map>
#include <vector>
#include <optional>
#include <mutex>
#include <atomic>

// 前向声明
namespace SQLite {
    class Database;
}

// 节点实时状态结构体
struct NodeStatus {
    std::string status = "offline";
    int64_t updated_at = 0;
};

/**
 * DatabaseManager类 - 数据库管理器
 * 
 * 负责管理SQLite数据库的连接和操作
 */
class DatabaseManager {
public:
    // 构造与析构
    DatabaseManager() = default;
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // 数据库初始化
    bool initialize();
    bool initializeBusinessTables();
    bool initializeMetricTables();
    bool initializeNodeTables();

    // 节点监控相关
    bool saveNode(const nlohmann::json& node_info);
    void updateNodeLastSeen(const std::string& node_id);
    void updateNodeStatus(const std::string& node_id, const std::string& status);
    NodeStatus getNodeStatus(const std::string& node_id);

    // 节点监控与资源采集
    void startNodeStatusMonitor();
    bool saveResourceUsage(const nlohmann::json& resource_usage);
    bool saveCpuMetrics(const std::string& node_id, long long timestamp, const nlohmann::json& cpu_data);
    bool saveMemoryMetrics(const std::string& node_id, long long timestamp, const nlohmann::json& memory_data);

    nlohmann::json getNodes();
    nlohmann::json getNode(const std::string& node_id);
    nlohmann::json getCpuMetrics(const std::string& node_id);
    nlohmann::json getMemoryMetrics(const std::string& node_id);

    // 业务管理相关
    bool saveBusiness(const nlohmann::json& business_info);
    bool updateBusinessStatus(const std::string& business_id, const std::string& status);
    bool saveBusinessComponent(const nlohmann::json& component_info);
    bool updateComponentStatus(const nlohmann::json& component_info);
    bool updateComponentStatus(const std::string& component_id, const std::string& status);
    bool updateComponentStatus(const std::string& component_id, const std::string& type, const std::string& status, const std::string& container_id = "", const std::string& process_id = "");
    bool saveComponentMetrics(const std::string& component_id, long long timestamp, const nlohmann::json& metrics);
    int countAbnormalComponents(const std::string& business_id);
    nlohmann::json getBusinesses();
    nlohmann::json getBusinessDetails(const std::string& business_id);
    nlohmann::json getBusinessComponents(const std::string& business_id);
    nlohmann::json getComponentDetails(const std::string& component_id);
    nlohmann::json getComponentMetrics(const std::string& component_id, int limit = 100);
    bool deleteBusiness(const std::string& business_id);

    // 资源调度相关
    nlohmann::json getNodeResourceInfo(const std::string& node_id);

    // 模板管理相关
    bool createBusinessTemplateTable();
    bool createComponentTemplateTable();
    nlohmann::json getBusinessTemplate(const std::string& template_id);
    nlohmann::json getBusinessTemplates();
    nlohmann::json saveBusinessTemplate(const nlohmann::json& template_info);
    nlohmann::json deleteBusinessTemplate(const std::string& template_id);
    nlohmann::json saveComponentTemplate(const nlohmann::json& template_info);
    nlohmann::json getComponentTemplates();
    nlohmann::json getComponentTemplate(const std::string& template_id);
    nlohmann::json deleteComponentTemplate(const std::string& template_id);

    nlohmann::json getComponentById(const std::string& component_id);

    nlohmann::json getComponentsByNodeId(const std::string& node_id);

    nlohmann::json getOnlineNodes();

private:
    std::string db_path_;                     // 数据库文件路径
    std::unique_ptr<SQLite::Database> db_;    // 数据库连接

    bool node_monitor_running_;               // 节点监控线程运行标志
    std::unique_ptr<std::thread> node_monitor_thread_; // 节点监控线程

    // 节点状态内存缓存
    std::unordered_map<std::string, NodeStatus> node_status_map_;
    std::mutex node_status_mutex_;

    // Slot Status Monitor
    std::unique_ptr<std::thread> slot_status_monitor_thread_;
    std::atomic<bool> slot_status_monitor_running_{false}; // Initialize to false
};

// 告警规则的C++结构体 (与表字段对应)
struct AlarmRule {
    int id;
    std::string alarm_name;
    int alarm_type;
    int alarm_level;
    std::string metric_key;
    std::string comparison_operator;
    std::string threshold_value;
    std::string secondary_threshold_value;
    int trigger_count;
    std::string target_identifier;
    std::string description;
    std::string created_at;
    std::string updated_at;
};

#endif // DATABASE_MANAGER_H
