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

/**
 * DatabaseManager类 - 数据库管理器
 * 
 * 负责管理SQLite数据库的连接和操作
 */
class DatabaseManager {
public:
    // 构造与析构
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // 数据库初始化
    bool initialize();
    bool initializeBusinessTables();
    bool initializeMetricTables();
    bool initializeChassisAndSlots();
    bool initializeBoardTables();

    // 资源监控相关
    bool saveBoard(const nlohmann::json& board_info);
    bool updateBoardLastSeen(const std::string& board_id);
    bool updateBoardStatus(const std::string& board_id, const std::string& status);

    // 节点监控与资源采集
    void startNodeStatusMonitor();
    bool saveResourceUsage(const nlohmann::json& resource_usage);
    bool saveCpuMetrics(const std::string& board_id, long long timestamp, const nlohmann::json& cpu_data);
    bool saveMemoryMetrics(const std::string& board_id, long long timestamp, const nlohmann::json& memory_data);
    bool saveDiskMetrics(const std::string& board_id, long long timestamp, const nlohmann::json& disk_data);
    bool saveNetworkMetrics(const std::string& board_id, long long timestamp, const nlohmann::json& network_data);
    bool saveDockerMetrics(const std::string& board_id, long long timestamp, const nlohmann::json& docker_data);

    nlohmann::json getBoards();
    nlohmann::json getBoard(const std::string& board_id);
    nlohmann::json getCpuMetrics(const std::string& board_id, int limit = 100);
    nlohmann::json getMemoryMetrics(const std::string& board_id, int limit = 100);
    nlohmann::json getDiskMetrics(const std::string& board_id, int limit = 100);
    nlohmann::json getNetworkMetrics(const std::string& board_id, int limit = 100);
    nlohmann::json getDockerMetrics(const std::string& board_id, int limit = 100);
    nlohmann::json getNodeResourceHistory(const std::string& node_id, int limit = 100);

    // 业务管理相关
    bool saveBusiness(const nlohmann::json& business_info);
    bool updateBusinessStatus(const std::string& business_id, const std::string& status);
    bool saveBusinessComponent(const nlohmann::json& component_info);
    bool updateComponentStatus(const std::string& component_id, const std::string& status, const std::string& container_id = "");
    bool saveComponentMetrics(const std::string& component_id, long long timestamp, const nlohmann::json& metrics);
    nlohmann::json getBusinesses();
    nlohmann::json getBusinessDetails(const std::string& business_id);
    nlohmann::json getBusinessComponents(const std::string& business_id);
    nlohmann::json getComponentDetails(const std::string& component_id);
    nlohmann::json getComponentMetrics(const std::string& component_id, int limit = 100);
    bool deleteBusiness(const std::string& business_id);

    // 资源调度相关
    nlohmann::json getNodeResourceInfo(const std::string& node_id);
    nlohmann::json getNodeGpuInfo(const std::string& node_id);

    // 模板管理相关
    bool createBusinessTemplateTable();
    bool createComponentTemplateTable();
    nlohmann::json getBusinessTemplate(const std::string& template_id);
    nlohmann::json getBusinessTemplates();
    nlohmann::json saveBusinessTemplate(const nlohmann::json& template_info);
    nlohmann::json updateBusinessTemplate(const std::string& template_id, const nlohmann::json& template_info);
    nlohmann::json deleteBusinessTemplate(const std::string& template_id);
    nlohmann::json saveComponentTemplate(const nlohmann::json& template_info);
    nlohmann::json getComponentTemplates();
    nlohmann::json getComponentTemplate(const std::string& template_id);
    nlohmann::json deleteComponentTemplate(const std::string& template_id);

    // 集群资源聚合
    nlohmann::json getClusterMetrics();
    nlohmann::json getClusterMetricsHistory(int limit = 100);

    // 告警规则相关
    bool createAlarmRulesTable();
    nlohmann::json addAlarmRule(const nlohmann::json& rule);
    nlohmann::json deleteAlarmRule(int id);
    nlohmann::json updateAlarmRule(int id, const nlohmann::json& rule);
    nlohmann::json getAlarmRule(int id);
    nlohmann::json getAlarmRules();

    // Chassis and Slot Management
    bool saveChassis(const nlohmann::json& chassis_info);
    bool saveSlot(const nlohmann::json& slot_info);
    bool updateSlotStatusOnly(const std::string& hostIp, const std::string& new_status);
    void startSlotStatusMonitorThread();
    
    // Node Management
    bool saveNode(const nlohmann::json& node_info);
    bool updateNode(const nlohmann::json& node_info);
    nlohmann::json getNode(int box_id, int slot_id, int cpu_id);
    nlohmann::json getNodeByHostIp(const std::string& hostIp);
    nlohmann::json getAllNodes();
    
    // Chassis and Slot Query Methods
    nlohmann::json getSlots();
    nlohmann::json getSlot(const std::string& chassis_id, int slot_index);
    nlohmann::json getChassisList();
    nlohmann::json getChassisInfo(const std::string& chassis_id);
    nlohmann::json getChassisDetailedList();
    nlohmann::json getSlotsWithLatestMetrics();

    // Slot Metrics Methods
    bool saveSlotCpuMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& cpu_data);
    bool saveSlotMemoryMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& memory_data);
    bool saveSlotDiskMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& disk_data);
    bool saveSlotNetworkMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& network_data);
    bool saveSlotDockerMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& docker_data);
    bool saveSlotGpuMetrics(const std::string& hostIp, long long timestamp, const nlohmann::json& gpu_data);
    
    nlohmann::json getSlotCpuMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getSlotMemoryMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getSlotDiskMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getSlotNetworkMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getSlotDockerMetrics(const std::string& hostIp, int limit = 100);
    nlohmann::json getSlotGpuMetrics(const std::string& hostIp, int limit = 100);
    
    bool saveSlotResourceUsage(const nlohmann::json& resource_usage);
    bool updateSlotMetrics(const nlohmann::json& metrics_data);

private:
    std::string db_path_;                     // 数据库文件路径
    std::unique_ptr<SQLite::Database> db_;    // 数据库连接

    bool node_monitor_running_;               // 节点监控线程运行标志
    std::unique_ptr<std::thread> node_monitor_thread_; // 节点监控线程

    // Slot Status Monitor
    std::unique_ptr<std::thread> slot_status_monitor_thread_;
    std::atomic<bool> slot_status_monitor_running_{false}; // Initialize to false
    void slotStatusMonitorLoop(); // Method to be run by slot_status_monitor_thread_
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

extern std::unordered_map<std::string, std::vector<AlarmRule>> g_cached_rules;
extern std::mutex g_alarm_rule_mutex;
void loadEnabledAlarmRulesToCache(class SQLite::Database* db);
void refreshAlarmRuleCache(class SQLite::Database* db);

#endif // DATABASE_MANAGER_H
