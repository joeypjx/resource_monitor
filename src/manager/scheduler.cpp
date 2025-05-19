#include "scheduler.h"
#include "database_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

Scheduler::Scheduler(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {
}

Scheduler::~Scheduler() {
}

bool Scheduler::initialize() {
    std::cout << "Initializing Scheduler..." << std::endl;
    return true;
}

nlohmann::json Scheduler::scheduleComponents(const std::string& business_id, const nlohmann::json& components) {
    std::cout << "Scheduling components for business: " << business_id << std::endl;
    
    // 获取所有可用节点
    auto available_nodes = getAvailableNodes();
    
    if (available_nodes.empty()) {
        return {
            {"status", "error"},
            {"message", "No available nodes"}
        };
    }

    std::cout << "Available nodes: " << available_nodes.dump() << std::endl;
    
    // 调度结果
    nlohmann::json schedule_result = {
        {"status", "success"},
        {"business_id", business_id},
        {"component_schedules", nlohmann::json::array()}
    };
    
    // 为每个组件选择最佳节点
    for (const auto& component : components) {
        // 打印组件信息
        std::string component_id = component["component_id"];
        std::string component_type = component["type"];
        
        // 选择最佳节点
        std::string best_node_id = selectBestNodeForComponent(component, available_nodes);
        
        if (best_node_id.empty()) {
            return {
                {"status", "error"},
                {"message", "Failed to find suitable node for component: " + component_id}
            };
        }
        
        // 添加到调度结果
        schedule_result["component_schedules"].push_back({
            {"component_id", component_id},
            {"node_id", best_node_id},
            {"type", component_type}
        });
    }
    
    std::cout << "Schedule result: " << schedule_result.dump() << std::endl;

    return schedule_result;
}

nlohmann::json Scheduler::getAvailableNodes() {
    // 从数据库获取所有节点（用 agent 作为节点）
    auto agents = db_manager_->getAgents();

    // 过滤出可用节点
    nlohmann::json available_nodes = nlohmann::json::array();
    
    for (const auto& agent : agents) {
        // 检查节点是否在线（使用数据库中的status字段）
        if (agent.contains("status") && agent["status"] == "online") {
            std::string node_id = agent.contains("node_id") ? agent["node_id"].get<std::string>() : agent["agent_id"].get<std::string>();
            // 获取节点最新资源使用情况
            auto resource_usage = getNodeResourceUsage(node_id);
            // 添加资源使用情况
            nlohmann::json available_node = agent;
            available_node["node_id"] = node_id;
            available_node["resource_usage"] = resource_usage;
            // 添加到可用节点列表
            available_nodes.push_back(available_node);
        }
    }

    return available_nodes;
}

nlohmann::json Scheduler::getNodeResourceUsage(const std::string& node_id) {
    // 从数据库获取节点最新资源使用情况
    return db_manager_->getNodeResourceInfo(node_id);
}

bool Scheduler::checkNodeResourceRequirements(const std::string& node_id, const nlohmann::json& resource_requirements) {
    // 根据组件类型调用不同的检查方法
    if (resource_requirements.contains("type")) {
        std::string type = resource_requirements["type"];
        
        if (type == "docker") {
            return checkNodeResourceRequirementsForDocker(node_id, resource_requirements);
        } else if (type == "binary") {
            return checkNodeResourceRequirementsForBinary(node_id, resource_requirements);
        }
    }
    
    // 默认检查方法
    auto resource_usage = getNodeResourceUsage(node_id);
    
    // 检查CPU
    if (resource_requirements.contains("cpu") && resource_usage.contains("cpu_usage_percent")) {
        float required_cpu = resource_requirements["cpu"];
        float available_cpu = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
        
        if (required_cpu > available_cpu) {
            return false;
        }
    }
    
    // 检查内存
    if (resource_requirements.contains("memory") && resource_usage.contains("memory_usage_percent")) {
        float required_memory = resource_requirements["memory"];
        float available_memory = 100.0f - resource_usage["memory_usage_percent"].get<float>();
        
        if (required_memory > available_memory) {
            return false;
        }
    }
    
    // 检查GPU
    if (resource_requirements.contains("gpu") && resource_requirements["gpu"].get<bool>()) {
        // 检查节点是否有GPU
        if (!resource_usage.contains("gpu") || !resource_usage["gpu"].get<bool>()) {
            return false;
        }
    }
    
    return true;
}

bool Scheduler::checkNodeResourceRequirementsForDocker(const std::string& node_id, const nlohmann::json& resource_requirements) {
    auto resource_usage = getNodeResourceUsage(node_id);
    
    // 检查Docker是否可用 todo
    // if (!resource_usage.contains("docker_available") || !resource_usage["docker_available"].get<bool>()) {
    //     return false;
    // }
    
    // 检查CPU
    if (resource_requirements.contains("cpu") && resource_usage.contains("cpu_usage_percent")) {
        float required_cpu = resource_requirements["cpu"];
        float available_cpu = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
        // 打印CPU需求
        std::cout << "Required CPU: " << required_cpu << std::endl;
        // 打印CPU可用
        std::cout << "Available CPU: " << available_cpu << std::endl;
        
        if (required_cpu > available_cpu) {
            return false;
        }
    }
    
    // 检查内存
    if (resource_requirements.contains("memory") && resource_usage.contains("memory_usage_percent")) {
        float required_memory = resource_requirements["memory"];
        float available_memory = 100.0f - resource_usage["memory_usage_percent"].get<float>();
        // 打印内存需求
        std::cout << "Required Memory: " << required_memory << std::endl;
        // 打印内存可用
        std::cout << "Available Memory: " << available_memory << std::endl;
        
        if (required_memory > available_memory) {
            return false;
        }
    }
    
    // 检查GPU
    if (resource_requirements.contains("gpu") && resource_requirements["gpu"].get<bool>()) {
        // 检查节点是否有GPU
        if (!resource_usage.contains("gpu") || !resource_usage["gpu"].get<bool>()) {
            return false;
        }
    }
    
    return true;
}

bool Scheduler::checkNodeResourceRequirementsForBinary(const std::string& node_id, const nlohmann::json& resource_requirements) {
    auto resource_usage = getNodeResourceUsage(node_id);
    
    // 检查CPU
    if (resource_requirements.contains("cpu") && resource_usage.contains("cpu_usage_percent")) {
        float required_cpu = resource_requirements["cpu"];
        float available_cpu = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
        
        if (required_cpu > available_cpu) {
            return false;
        }
    }
    
    // 检查内存
    if (resource_requirements.contains("memory") && resource_usage.contains("memory_usage_percent")) {
        float required_memory = resource_requirements["memory"];
        float available_memory = 100.0f - resource_usage["memory_usage_percent"].get<float>();
        
        if (required_memory > available_memory) {
            return false;
        }
    }
    
    // 检查GPU
    if (resource_requirements.contains("gpu") && resource_requirements["gpu"].get<bool>()) {
        // 检查节点是否有GPU
        if (!resource_usage.contains("gpu") || !resource_usage["gpu"].get<bool>()) {
            return false;
        }
    }
    
    return true;
}

bool Scheduler::checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity) {
    auto resource_usage = getNodeResourceUsage(node_id);
    
    // 检查GPU亲和性
    if (affinity.contains("gpu") && affinity["gpu"].get<bool>()) {
        // 检查节点是否有GPU
        if (!resource_usage.contains("gpu") || !resource_usage["gpu"].get<bool>()) {
            return false;
        }
    }
    
    return true;
}

std::string Scheduler::selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes) {
    std::string best_node_id;
    float best_score = -1.0f;
    
    // 获取组件资源需求
    nlohmann::json resource_requirements;
    if (component.contains("resource_requirements")) {
        resource_requirements = component["resource_requirements"];
    }
    
    // 获取组件亲和性
    nlohmann::json affinity;
    if (component.contains("affinity")) {
        affinity = component["affinity"];
    }
    
    // 添加组件类型到资源需求
    if (component.contains("type")) {
        resource_requirements["type"] = component["type"];
    }
    
    // 为每个节点计算得分
    for (const auto& node : available_nodes) {
        std::string node_id = node["agent_id"];
        // best_node_id = node_id; // todo 需要修改
        
        // 检查资源需求
        if (!checkNodeResourceRequirements(node_id, resource_requirements)) {
            // 打印资源需求
            std::cout << "Resource requirements: " << resource_requirements.dump() << std::endl;
            // 打印节点资源使用情况
            std::cout << "Node resource usage: " << node["resource_usage"].dump() << std::endl;
            continue;
        }
        
        // 检查亲和性
        if (!checkNodeAffinity(node_id, affinity)) {
            // 打印亲和性
            std::cout << "Affinity: " << affinity.dump() << std::endl;
            // 打印节点资源使用情况
            std::cout << "Node resource usage: " << node["resource_usage"].dump() << std::endl;
            continue;
        }
        
        // 计算负载均衡得分
        float cpu_score = 0.0f;
        float memory_score = 0.0f;
        
        if (node["resource_usage"].contains("cpu_usage_percent")) {
            cpu_score = 100.0f - node["resource_usage"]["cpu_usage_percent"].get<float>();
        }
        
        if (node["resource_usage"].contains("memory_usage_percent")) {
            memory_score = 100.0f - node["resource_usage"]["memory_usage_percent"].get<float>();
        }
        
        // 综合得分（CPU和内存各占50%）
        float score = 0.5f * cpu_score + 0.5f * memory_score;
        
        // 打印得分
        std::cout << "Score: " << score << std::endl;
        // 打印节点ID
        std::cout << "Node ID: " << node_id << std::endl;
        // 打印节点资源使用情况
        std::cout << "Node resource usage: " << node["resource_usage"].dump() << std::endl;

        // 更新最佳节点
        if (score > best_score) {
            best_score = score;
            best_node_id = node_id;
        }

        // best_node_id = node_id; // todo 需要修改
    }
    
    std::cout << "Best node for component " << component["component_id"] << ": " << best_node_id << std::endl; // todo 需要修改

    return best_node_id;
}
