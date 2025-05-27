#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <map>
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
     * 获取所有可用节点
     * 
     * @return 节点列表
     */
    nlohmann::json getAvailableNodes();
    
    /**
     * 获取节点最新资源使用情况
     * 
     * @param node_id 节点ID
     * @return 资源使用情况
     */
    nlohmann::json getNodeResourceUsage(const std::string& node_id);
    
    /**
     * 获取节点完整信息
     * 
     * @param node_id 节点ID
     * @return 节点完整信息
     */
    nlohmann::json getNodeInfo(const std::string& node_id);
    
    /**
     * 检查节点是否满足组件资源需求
     * 
     * @param node_id 节点ID
     * @param resource_requirements 资源需求
     * @return 是否满足
     */
    bool checkNodeResourceRequirements(const std::string& node_id, const nlohmann::json& resource_requirements);
    
    /**
     * 检查节点是否满足Docker组件资源需求
     * 
     * @param node_id 节点ID
     * @param resource_requirements 资源需求
     * @return 是否满足
     */
    bool checkNodeResourceRequirementsForDocker(const std::string& node_id, const nlohmann::json& resource_requirements);
    
    /**
     * 检查节点是否满足二进制组件资源需求
     * 
     * @param node_id 节点ID
     * @param resource_requirements 资源需求
     * @return 是否满足
     */
    bool checkNodeResourceRequirementsForBinary(const std::string& node_id, const nlohmann::json& resource_requirements);
    
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
    std::string selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器
    std::map<std::string, nlohmann::json> node_resources_;  // 节点资源缓存
};

#endif // SCHEDULER_H
