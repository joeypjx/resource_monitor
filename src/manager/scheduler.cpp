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

    return schedule_result;
}

nlohmann::json Scheduler::getAvailableNodes() {
    // 从数据库获取所有节点（用 board 作为节点）
    auto nodes = db_manager_->getNodes();

    // 过滤出可用节点
    nlohmann::json available_nodes = nlohmann::json::array();
    
    for (const auto& node : nodes) {
        // 检查节点是否在线（使用数据库中的status字段）
        if (node.contains("status") && node["status"] == "online") {
            // 添加到可用节点列表
            available_nodes.push_back(node);
        }
    }

    return available_nodes;
}

nlohmann::json Scheduler::getNodeResourceUsage(const std::string& node_id) {
    // 从数据库获取节点最新资源使用情况
    return db_manager_->getNodeResourceInfo(node_id);
}

nlohmann::json Scheduler::getNodeInfo(const std::string& node_id) {
    // 从数据库获取节点完整信息
    return db_manager_->getNode(node_id);
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
    if (affinity.empty()) {
        return true;
    }
    
    // 获取节点完整信息
    auto node_info = getNodeInfo(node_id);
    if (node_info.empty()) {
        std::cout << "Failed to get node info for: " << node_id << std::endl;
        return false;
    }
    
    // 遍历所有亲和性条件
    for (const auto& [key, value] : affinity.items()) {
        // 特殊处理GPU亲和性（向后兼容）
        if (key == "gpu" || key == "require_gpu") {
            if (value.get<bool>()) {
                auto resource_usage = getNodeResourceUsage(node_id);
                if (!resource_usage.contains("gpu") || !resource_usage["gpu"].get<bool>()) {
                    std::cout << "Node " << node_id << " does not meet GPU requirement" << std::endl;
                    return false;
                }
            }
            continue;
        }
        
        // 检查IP地址匹配
        if (key == "ip_address" || key == "ip") {
            if (node_info.contains("ip_address")) {
                std::string node_ip = node_info["ip_address"].get<std::string>();
                std::string required_ip = value.get<std::string>();
                if (node_ip != required_ip) {
                    std::cout << "Node " << node_id << " IP (" << node_ip 
                              << ") does not match required IP (" << required_ip << ")" << std::endl;
                    return false;
                }
            } else {
                std::cout << "Node " << node_id << " does not have IP address info" << std::endl;
                return false;
            }
            continue;
        }
        
        // 检查主机名匹配
        if (key == "hostname") {
            if (node_info.contains("hostname")) {
                std::string node_hostname = node_info["hostname"].get<std::string>();
                std::string required_hostname = value.get<std::string>();
                if (node_hostname != required_hostname) {
                    std::cout << "Node " << node_id << " hostname (" << node_hostname 
                              << ") does not match required hostname (" << required_hostname << ")" << std::endl;
                    return false;
                }
            } else {
                std::cout << "Node " << node_id << " does not have hostname info" << std::endl;
                return false;
            }
            continue;
        }
        
        // 检查node_id匹配
        if (key == "node_id") {
            std::string required_node_id = value.get<std::string>();
            if (node_id != required_node_id) {
                std::cout << "Node " << node_id << " does not match required node_id ("
                          << required_node_id << ")" << std::endl;
                return false;
            }
            continue;
        }
        
        // 检查操作系统匹配
        if (key == "os_info" || key == "os") {
            if (node_info.contains("os_info")) {
                std::string node_os = node_info["os_info"].get<std::string>();
                std::string required_os = value.get<std::string>();
                // 支持部分匹配（包含关系）
                if (node_os.find(required_os) == std::string::npos) {
                    std::cout << "Node " << node_id << " OS (" << node_os 
                              << ") does not match required OS (" << required_os << ")" << std::endl;
                    return false;
                }
            } else {
                std::cout << "Node " << node_id << " does not have OS info" << std::endl;
                return false;
            }
            continue;
        }
        
        // 检查节点标签匹配（如果节点有标签系统的话）
        if (key == "labels") {
            if (node_info.contains("labels") && value.is_object()) {
                for (const auto& [label_key, label_value] : value.items()) {
                    if (!node_info["labels"].contains(label_key) || 
                        node_info["labels"][label_key] != label_value) {
                        std::cout << "Node " << node_id << " does not have required label: " 
                                  << label_key << "=" << label_value.dump() << std::endl;
                        return false;
                    }
                }
            } else {
                std::cout << "Node " << node_id << " does not have labels or affinity labels not properly formatted" << std::endl;
                return false;
            }
            continue;
        }
        
        // 通用字段匹配
        if (node_info.contains(key)) {
            if (node_info[key] != value) {
                std::cout << "Node " << node_id << " field " << key 
                          << " (" << node_info[key].dump() << ") does not match required value (" 
                          << value.dump() << ")" << std::endl;
                return false;
            }
        } else {
            std::cout << "Node " << node_id << " does not have field: " << key << std::endl;
            return false;
        }
    }
    
    std::cout << "Node " << node_id << " meets all affinity requirements" << std::endl;
    return true;
}

std::string Scheduler::selectBestNodeForComponent(const nlohmann::json& component, const nlohmann::json& available_nodes) {
    std::string best_node_id;
    float best_score = -1.0f;
    
    // 获取组件亲和性
    nlohmann::json affinity;
    if (component.contains("affinity")) {
        affinity = component["affinity"];
    }
    
    // 为每个节点计算得分
    for (const auto& node : available_nodes) {
        // 打印节点信息
        std::string node_id = node["node_id"];
        
        // 检查亲和性
        if (!checkNodeAffinity(node_id, affinity)) {
            // 打印亲和性
            std::cout << "Affinity: " << affinity.dump() << std::endl;
            // 打印节点资源使用情况
            std::cout << "Node resource usage: " << node["resource_usage"].dump() << std::endl;
            continue;
        }

        nlohmann::json resource_usage = getNodeResourceUsage(node_id);

        // 计算负载均衡得分
        float cpu_score = 0.0f;
        float memory_score = 0.0f;
        
        if (resource_usage.contains("cpu_usage_percent")) {
            cpu_score = 100.0f - resource_usage["cpu_usage_percent"].get<float>();
        }
        
        if (resource_usage.contains("memory_usage_percent")) {
            memory_score = 100.0f - resource_usage["memory_usage_percent"].get<float>();
        }
        
        // 综合得分（CPU和内存各占50%）
        float score = 0.5f * cpu_score + 0.5f * memory_score;
        
        // 打印得分
        std::cout << "Node ID: " << node_id << " score: " << score << std::endl;

        // 更新最佳节点
        if (score > best_score) {
            best_score = score;
            best_node_id = node_id;
        }
    }
    
    std::cout << "Best node for component " << component["component_id"] << ": " << best_node_id << std::endl; // todo 需要修改

    return best_node_id;
}
