#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>

// 前向声明
class DatabaseManager;

/**
 * Scheduler类 - 资源调度器
 * 
 * 负责根据节点资源情况和组件需求进行调度
 */
class Scheduler {
public:
    /**
     * 默认构造函数
     */
    Scheduler() = default;
    
    /**
     * 构造函数
     * 
     * @param db_manager 数据库管理器
     */
    explicit Scheduler(std::shared_ptr<DatabaseManager> db_manager);
    
    /**
     * 析构函数
     */
    ~Scheduler();
    
    /**
     * 初始化调度器
     * 
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * 为业务组件选择最佳节点
     * 
     * @param business_id 业务ID
     * @param components 组件列表
     * @return 调度结果，包含组件到节点的映射
     */
    nlohmann::json scheduleComponents(const std::string& business_id, const nlohmann::json& components);

private:
    /**
     * 检查节点是否满足组件亲和性需求
     * 
     * @param node_id 节点ID
     * @param affinity 亲和性需求
     * @return 是否满足
     */
    bool checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity);
    
    /**
     * 根据负载均衡策略为组件选择最佳节点
     * 
     * @param component 组件信息
     * @param available_nodes 可用节点列表
     * @return 最佳节点ID
     */
    std::string selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes, std::unordered_map<std::string, int>& node_assign_count);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器
};

#endif // SCHEDULER_H
