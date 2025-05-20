#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

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
    /**
     * 构造函数
     * 
     * @param db_path 数据库文件路径
     */
    explicit DatabaseManager(const std::string& db_path);
    
    /**
     * 析构函数
     */
    ~DatabaseManager();
    
    /**
     * 初始化数据库
     * 
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * 初始化业务相关的数据库表
     * 
     * @return 是否成功初始化
     */
    bool initializeBusinessTables();
    
    // 资源监控相关方法
    bool saveAgent(const nlohmann::json& agent_info);
    bool updateAgentLastSeen(const std::string& agent_id);
    bool updateAgentStatus(const std::string& agent_id, const std::string& status);
    
    /**
     * 启动节点状态监控线程
     * 定期检查节点最后上报时间，超过阈值则标记为离线
     */
    void startNodeStatusMonitor();
    bool saveResourceUsage(const nlohmann::json& resource_usage);
    bool saveCpuMetrics(const std::string& agent_id, long long timestamp, const nlohmann::json& cpu_data);
    bool saveMemoryMetrics(const std::string& agent_id, long long timestamp, const nlohmann::json& memory_data);
    bool saveDiskMetrics(const std::string& agent_id, long long timestamp, const nlohmann::json& disk_data);
    bool saveNetworkMetrics(const std::string& agent_id, long long timestamp, const nlohmann::json& network_data);
    bool saveDockerMetrics(const std::string& agent_id, long long timestamp, const nlohmann::json& docker_data);
    
    nlohmann::json getAgents();
    nlohmann::json getNode(const std::string& node_id);
    nlohmann::json getCpuMetrics(const std::string& agent_id, int limit = 100);
    nlohmann::json getMemoryMetrics(const std::string& agent_id, int limit = 100);
    nlohmann::json getDiskMetrics(const std::string& agent_id, int limit = 100);
    nlohmann::json getNetworkMetrics(const std::string& agent_id, int limit = 100);
    nlohmann::json getDockerMetrics(const std::string& agent_id, int limit = 100);
    nlohmann::json getNodeResourceHistory(const std::string& node_id, int limit = 100);
    
    // 业务部署相关方法
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
    
    // 资源调度相关方法
    nlohmann::json getNodeResourceInfo(const std::string& node_id);
    nlohmann::json getNodeGpuInfo(const std::string& node_id);

    // 业务模板相关方法
    bool createBusinessTemplateTable();
    bool createComponentTemplateTable();
    nlohmann::json getBusinessTemplate(const std::string& template_id);
    nlohmann::json getBusinessTemplates();
    nlohmann::json saveBusinessTemplate(const nlohmann::json& template_info);
    nlohmann::json updateBusinessTemplate(const std::string& template_id, const nlohmann::json& template_info);
    nlohmann::json deleteBusinessTemplate(const std::string& template_id);

    // 组件/业务模板相关接口
    nlohmann::json saveComponentTemplate(const nlohmann::json& template_info);
    nlohmann::json getComponentTemplates();
    nlohmann::json getComponentTemplate(const std::string& template_id);
    nlohmann::json deleteComponentTemplate(const std::string& template_id);

    // 集群资源聚合
    nlohmann::json getClusterMetrics();

    // 集群资源历史聚合
    nlohmann::json getClusterMetricsHistory(int limit = 100);

private:
    std::string db_path_;                     // 数据库文件路径
    std::unique_ptr<SQLite::Database> db_;    // 数据库连接

    bool node_monitor_running_;               // 节点监控线程运行标志
    std::unique_ptr<std::thread> node_monitor_thread_; // 节点监控线程
};

#endif // DATABASE_MANAGER_H
