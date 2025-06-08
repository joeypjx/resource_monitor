#include "scheduler.h"
#include "database_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <climits>

Scheduler::Scheduler(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {}

Scheduler::~Scheduler() {}

bool Scheduler::initialize() {
    std::cout << "Initializing Scheduler..." << std::endl;
    return true;
}

nlohmann::json Scheduler::scheduleComponents(const std::string& business_id, const nlohmann::json& components) {
    // 获取所有可用节点
    auto available_nodes = getAvailableNodes();
    if (available_nodes.empty()) {
        return {{"status", "error"}, {"message", "No available nodes"}};
    }
    nlohmann::json schedule_result = {
        {"status", "success"},
        {"business_id", business_id},
        {"component_schedules", nlohmann::json::array()}
    };
    // 初始化节点分配计数
    std::unordered_map<std::string, int> node_assign_count;
    for (const auto& node : available_nodes) {
        node_assign_count[node["node_id"].get<std::string>()] = 0;
    }
    // 为每个组件选择最佳节点
    for (const auto& component : components) {
        std::string component_id = component["component_id"];
        std::string component_type = component["type"];
        std::string best_node_id = selectBestNodeForComponent(component, available_nodes, node_assign_count);
        if (best_node_id.empty()) {
            return {{"status", "error"}, {"message", "Failed to find suitable node for component: " + component_id}};
        }
        node_assign_count[best_node_id]++;
        schedule_result["component_schedules"].push_back({
            {"component_id", component_id},
            {"node_id", best_node_id},
            {"type", component_type}
        });
    }
    return schedule_result;
}

nlohmann::json Scheduler::getAvailableNodes() {
    auto nodes = db_manager_->getNodes();
    nlohmann::json available_nodes = nlohmann::json::array();
    for (const auto& node : nodes) {
        if (node.contains("status") && node["status"] == "online") {
            available_nodes.push_back(node);
        }
    }
    return available_nodes;
}

nlohmann::json Scheduler::getNodeResourceUsage(const std::string& node_id) {
    return db_manager_->getNodeResourceInfo(node_id);
}

nlohmann::json Scheduler::getNodeInfo(const std::string& node_id) {
    return db_manager_->getNode(node_id);
}

bool Scheduler::checkNodeAffinity(const std::string& node_id, const nlohmann::json& affinity) {
    if (affinity.empty()) return true;
    auto node_info = getNodeInfo(node_id);
    if (node_info.empty()) return false;
    for (const auto& [key, value] : affinity.items()) {
        if (key == "ip_address" || key == "ip") {
            if (node_info.contains("ip_address")) {
                std::string node_ip = node_info["ip_address"].get<std::string>();
                std::string required_ip = value.get<std::string>();
                if (node_ip != required_ip) return false;
            } else {
                return false;
            }
            continue;
        }
        if (node_info.contains(key)) {
            if (node_info[key] != value) return false;
        } else {
            return false;
        }
    }
    return true;
}

std::string Scheduler::selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes, std::unordered_map<std::string, int>& node_assign_count) {
    std::string best_node_id;
    float best_score = -1.0f;
    nlohmann::json affinity;
    if (component.contains("affinity")) affinity = component["affinity"];
    // 有亲和性，按亲和性优先分配
    if (!affinity.is_null() && !affinity.empty()) {
        for (const auto& node : available_nodes) {
            std::string node_id = node["node_id"];
            if (!checkNodeAffinity(node_id, affinity)) continue;
            nlohmann::json resource_usage = getNodeResourceUsage(node_id);
            float cpu_score = 0.0f, memory_score = 0.0f;
            if (resource_usage.contains("cpu_usage_percent")) cpu_score = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
            if (resource_usage.contains("memory_usage_percent")) memory_score = 100.0f - resource_usage["memory_usage_percent"].get<float>();
            float score = 0.5f * cpu_score + 0.5f * memory_score;
            if (score > best_score) {
                best_score = score;
                best_node_id = node_id;
            }
        }
        return best_node_id;
    }
    // 无亲和性，优先分散分配
    std::vector<std::string> unused_nodes;
    for (const auto& node : available_nodes) {
        std::string node_id = node["node_id"];
        if (node_assign_count[node_id] == 0) unused_nodes.push_back(node_id);
    }
    std::vector<std::string> candidate_nodes;
    if (!unused_nodes.empty()) {
        candidate_nodes = unused_nodes;
    } else {
        int min_count = INT_MAX;
        for (const auto& node : available_nodes) {
            std::string node_id = node["node_id"];
            if (node_assign_count[node_id] < min_count) min_count = node_assign_count[node_id];
        }
        for (const auto& node : available_nodes) {
            std::string node_id = node["node_id"];
            if (node_assign_count[node_id] == min_count) candidate_nodes.push_back(node_id);
        }
    }
    for (const auto& node : available_nodes) {
        std::string node_id = node["node_id"];
        if (std::find(candidate_nodes.begin(), candidate_nodes.end(), node_id) == candidate_nodes.end()) continue;
        nlohmann::json resource_usage = getNodeResourceUsage(node_id);
        float cpu_score = 0.0f, memory_score = 0.0f;
        if (resource_usage.contains("cpu_usage_percent")) cpu_score = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
        if (resource_usage.contains("memory_usage_percent")) memory_score = 100.0f - resource_usage["memory_usage_percent"].get<float>();
        float score = 0.5f * cpu_score + 0.5f * memory_score;
        if (score > best_score) {
            best_score = score;
            best_node_id = node_id;
        }
    }
    return best_node_id;
}