#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

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
     * 构造函数
     * 
     * @param db_manager 数据库管理器
     */
    explicit Scheduler(std::shared_ptr<DatabaseManager> db_manager);
    
    /**
     * 析构函数
     */
    ~Scheduler() = default;
    
    /**
     * 调度业务组件
     * 
     * @param business_info 业务信息
     * @return 调度结果，包含每个组件的目标节点
     */
    nlohmann::json scheduleComponents(const nlohmann::json& business_info);
    
    /**
     * 获取节点资源情况
     * 
     * @return 节点资源情况
     */
    nlohmann::json getNodesResourceStatus();
    
    /**
     * 检查节点是否满足组件资源需求
     * 
     * @param node_id 节点ID
     * @param resource_requirements 资源需求
     * @return 是否满足
     */
    bool checkNodeResourceRequirements(const std::string& node_id, 
                                      const nlohmann::json& resource_requirements);
    
    /**
     * 检查节点是否满足组件亲和性要求
     * 
     * @param node_id 节点ID
     * @param affinity 亲和性要求
     * @return 是否满足
     */
    bool checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity);
    
    /**
     * 选择最优节点
     * 
     * @param candidate_nodes 候选节点列表
     * @param resource_requirements 资源需求
     * @return 最优节点ID
     */
    std::string selectOptimalNode(const std::vector<std::string>& candidate_nodes, 
                                 const nlohmann::json& resource_requirements);

private:
    /**
     * 计算节点负载分数
     * 
     * @param node_id 节点ID
     * @return 负载分数（越低越好）
     */
    double calculateNodeLoadScore(const std::string& node_id);
    
    /**
     * 获取节点GPU信息
     * 
     * @param node_id 节点ID
     * @return GPU信息
     */
    nlohmann::json getNodeGpuInfo(const std::string& node_id);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器
    
    // 缓存节点资源情况，定期更新
    std::map<std::string, nlohmann::json> node_resources_;
    std::map<std::string, nlohmann::json> node_gpu_info_;
};

#endif // SCHEDULER_H
