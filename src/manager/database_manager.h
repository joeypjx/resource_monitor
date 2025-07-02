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
 * @struct NodeStatus
 * @brief 存储节点的实时状态信息。
 * 主要用于内存缓存，以快速查询节点是否在线。
 */
struct NodeStatus {
    std::string status = "offline"; // 节点状态，如 "online", "offline"
    int64_t updated_at = 0;         // 最后一次收到心跳的时间戳
};

/**
 * @class DatabaseManager
 * @brief 数据库管理器
 *
 * 封装了所有与SQLite数据库的交互。这是系统的数据访问层（DAO）。
 * - 负责数据库的初始化、表的创建。
 * - 提供了一系列接口用于增、删、改、查节点、业务、组件、模板和监控指标等数据。
 * - 内部维护一个节点状态的内存缓存(`node_status_map_`)，以提高状态查询性能并减少数据库负载。
 * - 启动一个后台线程(`node_monitor_thread_`)来周期性检查节点是否超时下线。
 */
class DatabaseManager {
public:
    // --- Constructor & Destructor ---
    
    /**
     * @brief 默认构造函数
     */
    DatabaseManager() = default;

    /**
     * @brief 构造函数
     * @param db_path SQLite数据库文件的路径
     */
    explicit DatabaseManager(const std::string& db_path);

    /**
     * @brief 析构函数
     * 确保后台线程被正确停止和清理。
     */
    ~DatabaseManager();

    // --- Database Initialization ---

    /**
     * @brief 初始化数据库
     * 创建所有必要的表。
     * @return bool 初始化成功返回true
     */
    bool initialize();

    /**
     * @brief 初始化业务相关的表
     * @return bool 成功返回true
     */
    bool initializeBusinessTables();

    /**
     * @brief 初始化监控指标相关的表
     * @return bool 成功返回true
     */
    bool initializeMetricTables();
    
    /**
     * @brief 初始化节点相关的表
     * @return bool 成功返回true
     */
    bool initializeNodeTables();

    // --- Node & Metric Management ---

    /**
     * @brief 保存新的节点信息到数据库
     * @param node_info 包含节点信息的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveNode(const nlohmann::json& node_info);

    /**
     * @brief 更新节点的最后一次心跳时间
     * @param node_id 节点的唯一ID
     */
    void updateNodeLastSeen(const std::string& node_id);

    /**
     * @brief 更新节点的状态（如 'online', 'offline'）
     * 同时更新数据库和内存缓存。
     * @param node_id 节点的唯一ID
     * @param status 新的状态
     */
    void updateNodeStatus(const std::string& node_id, const std::string& status);

    /**
     * @brief 从内存缓存中获取节点状态
     * @param node_id 节点的唯一ID
     * @return NodeStatus 节点状态结构体
     */
    NodeStatus getNodeStatus(const std::string& node_id);

    /**
     * @brief 启动节点状态监控线程
     * 该线程会周期性地检查并更新超时下线的节点。
     */
    void startNodeStatusMonitor();

    /**
     * @brief 保存来自Agent的资源使用情况数据
     * @param resource_usage 包含资源使用情况的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveResourceUsage(const nlohmann::json& resource_usage);

    /**
     * @brief 保存CPU指标数据
     * @param node_id 节点ID
     * @param timestamp 时间戳
     * @param cpu_data 包含CPU指标的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveCpuMetrics(const std::string& node_id, long long timestamp, const nlohmann::json& cpu_data);

    /**
     * @brief 保存内存指标数据
     * @param node_id 节点ID
     * @param timestamp 时间戳
     * @param memory_data 包含内存指标的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveMemoryMetrics(const std::string& node_id, long long timestamp, const nlohmann::json& memory_data);

    /**
     * @brief 获取所有节点的列表
     * @return nlohmann::json 包含所有节点信息的JSON数组
     */
    nlohmann::json getNodes();

    /**
     * @brief 获取单个节点的详细信息
     * @param node_id 节点ID
     * @return nlohmann::json 包含节点详细信息的JSON对象
     */
    nlohmann::json getNode(const std::string& node_id);

    /**
     * @brief 获取指定节点的CPU指标历史数据
     * @param node_id 节点ID
     * @return nlohmann::json 包含CPU指标的JSON数组
     */
    nlohmann::json getCpuMetrics(const std::string& node_id);

    /**
     * @brief 获取指定节点的内存指标历史数据
     * @param node_id 节点ID
     * @return nlohmann::json 包含内存指标的JSON数组
     */
    nlohmann::json getMemoryMetrics(const std::string& node_id);

    /**
     * @brief 获取所有在线的节点列表
     * @return nlohmann::json 包含在线节点信息的JSON数组
     */
    nlohmann::json getOnlineNodes();

    // --- Business & Component Management ---

    /**
     * @brief 保存业务信息到数据库
     * @param business_info 包含业务信息的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveBusiness(const nlohmann::json& business_info);

    /**
     * @brief 更新业务状态
     * @param business_id 业务ID
     * @param status 新的状态
     * @return bool 更新成功返回true
     */
    bool updateBusinessStatus(const std::string& business_id, const std::string& status);

    /**
     * @brief 保存业务组件信息到数据库
     * @param component_info 包含组件信息的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveBusinessComponent(const nlohmann::json& component_info);

    /**
     * @brief 更新组件状态（根据JSON对象）
     * @param component_info 包含组件ID和新状态的JSON对象
     * @return bool 更新成功返回true
     */
    bool updateComponentStatus(const nlohmann::json& component_info);

    /**
     * @brief 更新组件状态（根据ID和状态字符串）
     * @param component_id 组件ID
     * @param status 新的状态
     * @return bool 更新成功返回true
     */
    bool updateComponentStatus(const std::string& component_id, const std::string& status);

    /**
     * @brief 更新组件状态的详细信息
     * @param component_id 组件ID
     * @param type 组件类型 (e.g., 'docker', 'process')
     * @param status 新的状态
     * @param container_id Docker容器ID（如果适用）
     * @param process_id 进程ID（如果适用）
     * @return bool 更新成功返回true
     */
    bool updateComponentStatus(const std::string& component_id, const std::string& type, const std::string& status, const std::string& container_id = "", const std::string& process_id = "");
    
    /**
     * @brief 保存组件的性能指标
     * @param component_id 组件ID
     * @param timestamp 时间戳
     * @param metrics 包含性能指标的JSON对象
     * @return bool 保存成功返回true
     */
    bool saveComponentMetrics(const std::string& component_id, long long timestamp, const nlohmann::json& metrics);
    
    /**
     * @brief 统计指定业务下的异常组件数量
     * @param business_id 业务ID
     * @return int 异常组件的数量
     */
    int countAbnormalComponents(const std::string& business_id);

    /**
     * @brief 获取所有业务的列表
     * @return nlohmann::json 包含所有业务简要信息的JSON数组
     */
    nlohmann::json getBusinesses();

    /**
     * @brief 获取单个业务的详细信息
     * @param business_id 业务ID
     * @return nlohmann::json 包含业务详情（包括其下组件）的JSON对象
     */
    nlohmann::json getBusinessDetails(const std::string& business_id);

    /**
     * @brief 获取属于特定业务的所有组件
     * @param business_id 业务ID
     * @return nlohmann::json 包含组件信息的JSON数组
     */
    nlohmann::json getBusinessComponents(const std::string& business_id);

    /**
     * @brief 获取单个组件的详细信息
     * @param component_id 组件ID
     * @return nlohmann::json 包含组件详情的JSON对象
     */
    nlohmann::json getComponentDetails(const std::string& component_id);

    /**
     * @brief 获取组件的性能指标历史数据
     * @param component_id 组件ID
     * @param limit 返回的数据点数量限制
     * @return nlohmann::json 包含指标数据的JSON数组
     */
    nlohmann::json getComponentMetrics(const std::string& component_id, int limit = 100);

    /**
     * @brief 从数据库中删除一个业务及其关联组件
     * @param business_id 业务ID
     * @return bool 删除成功返回true
     */
    bool deleteBusiness(const std::string& business_id);

    // --- Resource Scheduling ---
    /**
     * @brief 获取节点的资源信息，用于调度决策
     * @param node_id 节点ID
     * @return nlohmann::json 包含节点资源摘要的JSON对象
     */
    nlohmann::json getNodeResourceInfo(const std::string& node_id);

    // --- Template Management ---
    /**
     * @brief 创建业务模板表
     * @return bool 成功返回true
     */
    bool createBusinessTemplateTable();

    /**
     * @brief 创建组件模板表
     * @return bool 成功返回true
     */
    bool createComponentTemplateTable();

    /**
     * @brief 获取单个业务模板的详细信息
     * @param template_id 模板ID
     * @return nlohmann::json 包含模板详情的JSON对象
     */
    nlohmann::json getBusinessTemplate(const std::string& template_id);

    /**
     * @brief 获取所有业务模板的列表
     * @return nlohmann::json 包含所有业务模板简要信息的JSON数组
     */
    nlohmann::json getBusinessTemplates();

    /**
     * @brief 保存（创建或更新）一个业务模板
     * @param template_info 包含模板信息的JSON对象
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json saveBusinessTemplate(const nlohmann::json& template_info);

    /**
     * @brief 删除一个业务模板
     * @param template_id 模板ID
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json deleteBusinessTemplate(const std::string& template_id);

    /**
     * @brief 保存（创建或更新）一个组件模板
     * @param template_info 包含模板信息的JSON对象
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json saveComponentTemplate(const nlohmann::json& template_info);

    /**
     * @brief 获取所有组件模板的列表
     * @return nlohmann::json 包含所有组件模板简要信息的JSON数组
     */
    nlohmann::json getComponentTemplates();

    /**
     * @brief 获取单个组件模板的详细信息
     * @param template_id 模板ID
     * @return nlohmann::json 包含模板详情的JSON对象
     */
    nlohmann::json getComponentTemplate(const std::string& template_id);

    /**
     * @brief 删除一个组件模板
     * @param template_id 模板ID
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json deleteComponentTemplate(const std::string& template_id);

    // --- Utility Methods ---
    /**
     * @brief 根据ID获取单个组件的信息
     * @param component_id 组件ID
     * @return nlohmann::json 包含组件信息的JSON对象
     */
    nlohmann::json getComponentById(const std::string& component_id);

    /**
     * @brief 根据节点ID获取该节点上的所有组件
     * @param node_id 节点ID
     * @return nlohmann::json 包含组件信息的JSON数组
     */
    nlohmann::json getComponentsByNodeId(const std::string& node_id);

private:
    std::string db_path_;                                 // 数据库文件路径
    std::unique_ptr<SQLite::Database> db_;                // SQLite数据库连接对象
    
    std::atomic<bool> node_monitor_running_{false};       // 节点监控线程运行标志
    std::unique_ptr<std::thread> node_monitor_thread_;    // 节点状态监控线程

    // 节点状态内存缓存
    std::unordered_map<std::string, NodeStatus> node_status_map_; // 节点ID到状态的映射
    std::mutex node_status_mutex_;                        // 用于保护缓存访问的互斥锁

    // Slot Status Monitor (用途待确认)
    std::atomic<bool> slot_status_monitor_running_{false};
    std::unique_ptr<std::thread> slot_status_monitor_thread_;
};

/**
 * @struct AlarmRule
 * @brief 告警规则的数据结构
 * 对应数据库中的 'alarm_rules' 表。
 */
struct AlarmRule {
    int id;                         // 规则ID
    std::string alarm_name;         // 告警名称
    int alarm_type;                 // 告警类型
    int alarm_level;                // 告警级别
    std::string metric_key;         // 监控指标的关键字
    std::string comparison_operator;// 比较操作符 (e.g., ">", "<", "=")
    std::string threshold_value;    // 阈值
    std::string secondary_threshold_value; // 第二阈值 (用于范围比较)
    int trigger_count;              // 触发次数
    std::string target_identifier;  // 告警目标标识
    std::string description;        // 描述
    std::string created_at;         // 创建时间
    std::string updated_at;         // 更新时间
};

#endif // DATABASE_MANAGER_H
