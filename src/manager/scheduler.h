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

// @class Scheduler
// @brief 资源调度器

// 负责为业务组件选择最合适的部署节点。这是系统的大脑，用于做出放置决策。
// 主要工作流程：
// 1. 接收一个包含多个组件的部署请求。
// 2. 从DatabaseManager获取所有可用（在线）的节点及其当前的资源信息。
// 3. 遍历每个组件，为其寻找最佳节点：
//    a. 首先，根据组件的"亲和性"设置（`affinity`）过滤掉不满足条件的节点。
//    b. 然后，在满足条件的节点中，根据负载均衡策略（当前是选择已被分配组件最少的节点）来选择最终节点。
// 4. 返回一个从组件ID到选定节点ID的映射。
class Scheduler {
public:
    // @brief 默认构造函数
    Scheduler() = default;
    
    // @brief 构造函数
    // @param db_manager 指向数据库管理器的共享指针，用于获取节点信息。
    explicit Scheduler(std::shared_ptr<DatabaseManager> db_manager);
    
    // @brief 析构函数
    ~Scheduler();
    
    // @brief 初始化调度器
    // @return bool 初始化成功返回true。
    bool initialize();
    
    // @brief 为一批业务组件选择最佳部署节点
    // 
    // @param business_id 业务ID，仅用于日志记录和追踪。
    // @param components 需要被调度的组件列表（JSON数组）。
    // @return nlohmann::json 一个JSON对象，包含了调度决策。
    //         成功时: `{"status": "success", "schedule": {"component_id_1": "node_id_A", ...}}`
    //         失败时: `{"status": "error", "message": "..."}`
    nlohmann::json scheduleComponents(const std::string& business_id, const nlohmann::json& components);

private:
    // @brief 检查一个节点是否满足组件的亲和性要求
    // 亲和性可以指定组件必须（或不能）部署在具有特定标签的节点上。
    // @param node_id 待检查的节点ID。
    // @param affinity 组件的亲和性配置（JSON对象）。
    // @return bool 如果节点满足所有亲和性规则，则返回true。
    bool checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity);
    
    // @brief 根据负载均衡策略为单个组件选择最佳节点
    // 当前的策略是选择一个可用节点中，到目前为止被分配组件数量最少的节点。
    // @param component 需要被调度的单个组件信息。
    // @param available_nodes 满足亲和性等条件的可用节点列表。
    // @param node_assign_count 一个map，用于跟踪每个节点已被分配的组件数量。
    // @return std::string 选定的最佳节点的ID。如果找不到合适的节点，则返回空字符串。
    std::string selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes, std::unordered_map<std::string, int>& node_assign_count);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器，用于访问节点信息。
};

#endif // SCHEDULER_H
