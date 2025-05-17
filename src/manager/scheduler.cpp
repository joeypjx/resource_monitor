#include "scheduler.h"
#include "database_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

Scheduler::Scheduler(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {
}

nlohmann::json Scheduler::scheduleComponents(const nlohmann::json& business_info) {
    nlohmann::json result;
    result["scheduled"] = nlohmann::json::array();
    result["failed"] = nlohmann::json::array();
    
    // 获取最新的节点资源情况
    auto nodes_status = getNodesResourceStatus();
    
    // 检查业务信息是否包含组件
    if (!business_info.contains("components") || !business_info["components"].is_array()) {
        std::cerr << "Business info does not contain components array" << std::endl;
        return result;
    }
    
    // 遍历所有组件进行调度
    for (const auto& component : business_info["components"]) {
        // 检查组件是否包含必要信息
        if (!component.contains("component_id") || !component.contains("resource_requirements")) {
            std::cerr << "Component missing required fields" << std::endl;
            continue;
        }
        
        std::string component_id = component["component_id"];
        nlohmann::json resource_requirements = component["resource_requirements"];
        nlohmann::json affinity = component.contains("affinity") ? component["affinity"] : nlohmann::json({});
        
        // 筛选满足资源需求和亲和性的节点
        std::vector<std::string> candidate_nodes;
        for (const auto& node_entry : nodes_status.items()) {
            std::string node_id = node_entry.key();
            
            // 检查节点是否满足资源需求和亲和性
            if (checkNodeResourceRequirements(node_id, resource_requirements) && 
                checkNodeAffinity(node_id, affinity)) {
                candidate_nodes.push_back(node_id);
            }
        }
        
        // 如果没有合适的节点，标记为调度失败
        if (candidate_nodes.empty()) {
            nlohmann::json failed_component;
            failed_component["component_id"] = component_id;
            failed_component["reason"] = "No suitable node found";
            result["failed"].push_back(failed_component);
            continue;
        }
        
        // 选择最优节点
        std::string selected_node = selectOptimalNode(candidate_nodes, resource_requirements);
        
        // 记录调度结果
        nlohmann::json scheduled_component;
        scheduled_component["component_id"] = component_id;
        scheduled_component["node_id"] = selected_node;
        result["scheduled"].push_back(scheduled_component);
        
        // 更新节点资源使用情况（简化处理，实际应该更精确地计算）
        if (resource_requirements.contains("cpu_cores")) {
            nodes_status[selected_node]["cpu_usage_percent"] = 
                nodes_status[selected_node]["cpu_usage_percent"].get<double>() + 
                (resource_requirements["cpu_cores"].get<double>() * 100.0 / 
                 nodes_status[selected_node]["cpu_core_count"].get<int>());
        }
        
        if (resource_requirements.contains("memory_mb")) {
            nodes_status[selected_node]["memory_used"] = 
                nodes_status[selected_node]["memory_used"].get<int64_t>() + 
                resource_requirements["memory_mb"].get<int>() * 1024 * 1024;
            
            nodes_status[selected_node]["memory_free"] = 
                nodes_status[selected_node]["memory_total"].get<int64_t>() - 
                nodes_status[selected_node]["memory_used"].get<int64_t>();
            
            nodes_status[selected_node]["memory_usage_percent"] = 
                (double)nodes_status[selected_node]["memory_used"].get<int64_t>() * 100.0 / 
                nodes_status[selected_node]["memory_total"].get<int64_t>();
        }
    }
    
    return result;
}

nlohmann::json Scheduler::getNodesResourceStatus() {
    nlohmann::json result;
    
    // 获取所有节点
    auto agents = db_manager_->getAgents();
    
    // 遍历所有节点，获取资源情况
    for (const auto& agent : agents) {
        std::string node_id = agent["agent_id"];
        result[node_id] = db_manager_->getNodeResourceInfo(node_id);
        
        // 获取GPU信息
        auto gpu_info = db_manager_->getNodeGpuInfo(node_id);
        result[node_id]["has_gpu"] = gpu_info["has_gpu"];
        result[node_id]["gpu_count"] = gpu_info["gpu_count"];
        
        // 缓存节点资源情况
        node_resources_[node_id] = result[node_id];
        node_gpu_info_[node_id] = gpu_info;
    }
    
    return result;
}

bool Scheduler::checkNodeResourceRequirements(const std::string& node_id, 
                                            const nlohmann::json& resource_requirements) {
    // 如果节点资源信息不存在，获取最新信息
    if (node_resources_.find(node_id) == node_resources_.end()) {
        node_resources_[node_id] = db_manager_->getNodeResourceInfo(node_id);
    }
    
    const auto& node_resource = node_resources_[node_id];
    
    // 检查CPU需求
    if (resource_requirements.contains("cpu_cores")) {
        double required_cores = resource_requirements["cpu_cores"].get<double>();
        int available_cores = node_resource["cpu_core_count"].get<int>();
        double current_usage_percent = node_resource["cpu_usage_percent"].get<double>();
        
        // 估算可用CPU核心数
        double available_core_percent = 100.0 - current_usage_percent;
        double available_cores_estimate = available_cores * available_core_percent / 100.0;
        
        if (available_cores_estimate < required_cores) {
            return false;
        }
    }
    
    // 检查内存需求
    if (resource_requirements.contains("memory_mb")) {
        int64_t required_memory = resource_requirements["memory_mb"].get<int>() * 1024 * 1024; // 转换为字节
        int64_t free_memory = node_resource["memory_free"].get<int64_t>();
        
        if (free_memory < required_memory) {
            return false;
        }
    }
    
    // 检查GPU需求
    if (resource_requirements.contains("gpu") && resource_requirements["gpu"].get<bool>()) {
        // 如果节点GPU信息不存在，获取最新信息
        if (node_gpu_info_.find(node_id) == node_gpu_info_.end()) {
            node_gpu_info_[node_id] = db_manager_->getNodeGpuInfo(node_id);
        }
        
        const auto& gpu_info = node_gpu_info_[node_id];
        
        if (!gpu_info["has_gpu"].get<bool>()) {
            return false;
        }
    }
    
    return true;
}

bool Scheduler::checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity) {
    // 如果没有亲和性要求，直接返回true
    if (affinity.is_null() || affinity.empty()) {
        return true;
    }
    
    // 检查GPU亲和性
    if (affinity.contains("require_gpu") && affinity["require_gpu"].get<bool>()) {
        // 如果节点GPU信息不存在，获取最新信息
        if (node_gpu_info_.find(node_id) == node_gpu_info_.end()) {
            node_gpu_info_[node_id] = db_manager_->getNodeGpuInfo(node_id);
        }
        
        const auto& gpu_info = node_gpu_info_[node_id];
        
        if (!gpu_info["has_gpu"].get<bool>()) {
            return false;
        }
    }
    
    // 检查节点标签亲和性（简化处理，实际应该有更复杂的标签匹配逻辑）
    if (affinity.contains("node_labels") && affinity["node_labels"].is_object()) {
        // 这里简化处理，假设我们没有实现节点标签功能
        // 实际应用中应该检查节点是否具有指定的标签
    }
    
    return true;
}

std::string Scheduler::selectOptimalNode(const std::vector<std::string>& candidate_nodes, 
                                       const nlohmann::json& resource_requirements) {
    // 如果只有一个候选节点，直接返回
    if (candidate_nodes.size() == 1) {
        return candidate_nodes[0];
    }
    
    // 计算每个节点的负载分数，选择负载最低的节点
    std::string optimal_node;
    double min_score = std::numeric_limits<double>::max();
    
    for (const auto& node_id : candidate_nodes) {
        double score = calculateNodeLoadScore(node_id);
        
        if (score < min_score) {
            min_score = score;
            optimal_node = node_id;
        }
    }
    
    return optimal_node;
}

double Scheduler::calculateNodeLoadScore(const std::string& node_id) {
    // 如果节点资源信息不存在，获取最新信息
    if (node_resources_.find(node_id) == node_resources_.end()) {
        node_resources_[node_id] = db_manager_->getNodeResourceInfo(node_id);
    }
    
    const auto& node_resource = node_resources_[node_id];
    
    // 计算CPU和内存的负载分数
    double cpu_score = node_resource["cpu_usage_percent"].get<double>();
    double memory_score = node_resource["memory_usage_percent"].get<double>();
    
    // 综合分数，CPU和内存各占50%权重
    return 0.5 * cpu_score + 0.5 * memory_score;
}
