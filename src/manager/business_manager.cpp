#include "business_manager.h"
#include "database_manager.h"
#include "scheduler.h"
#include <iostream>
#include <chrono>
#include <uuid/uuid.h>
#include <httplib.h>

// 生成UUID
std::string generate_uuid() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

BusinessManager::BusinessManager(std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<Scheduler> scheduler)
    : db_manager_(db_manager), scheduler_(scheduler) {
}

BusinessManager::~BusinessManager() {
}

bool BusinessManager::initialize() {
    std::cout << "Initializing BusinessManager..." << std::endl;
    return true;
}

nlohmann::json BusinessManager::deployBusiness(const nlohmann::json& business_info) {
    std::cout << "Deploying business..." << std::endl;
    
    // 验证业务信息
    if (!validateBusinessInfo(business_info)) {
        return {
            {"status", "error"},
            {"message", "Invalid business information"}
        };
    }
    
    // 生成业务ID（如果没有提供）
    std::string business_id;
    if (business_info.contains("business_id")) {
        business_id = business_info["business_id"];
    } else {
        business_id = generate_uuid();
    }

    // 保存业务信息（此时组件可以为空或只包含基本信息）
    // 新建一个业务对象
    nlohmann::json business = business_info;
    business["business_id"] = business_id;
    business["status"] = "running";
    business["created_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    business["updated_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    db_manager_->saveBusiness(business);
    
    // 获取组件列表
    if (!business_info.contains("components") || !business_info["components"].is_array()) {
        return {
            {"status", "error"},
            {"message", "Missing components"}
        };
    }
    
    auto components = business_info["components"];
    // 修改组件的id
    for (auto& component : components) {
        component["component_id"] = generate_uuid();
    }
    
    // 调度组件
    auto schedule_result = scheduler_->scheduleComponents(business_id, components);
    
    if (schedule_result["status"] != "success") {
        return schedule_result;
    }
    
    // 部署组件
    nlohmann::json deployed_components = nlohmann::json::array();
    
    for (const auto& schedule : schedule_result["component_schedules"]) {
        std::string component_id = schedule["component_id"];
        std::string node_id = schedule["node_id"];
        
        // 查找组件信息
        nlohmann::json component_info;
        for (const auto& component : components) {
            if (component["component_id"] == component_id) {
                component_info = component;
                break;
            }
        }
        
        // 部署组件
        auto deploy_result = deployComponent(business_id, component_info, node_id);

        if (deploy_result["status"] != "success") {
            // 停止已部署的组件
            for (const auto& deployed_component : deployed_components) {
                stopComponent(business_id, deployed_component["component_id"]);
            }
            
            return {
                {"status", "error"},
                {"message", "Failed to deploy component: " + component_id},
                {"detail", deploy_result}
            };
        }
        
        // 添加到已部署组件列表
        deployed_components.push_back({
            {"component_id", component_id},
            {"node_id", node_id},
            {"status", "running"}
        });
    }
    
    // 保存业务信息
    {
        std::lock_guard<std::mutex> lock(businesses_mutex_);
        
        businesses_[business_id] = {
            {"business_id", business_id},
            {"business_name", business_info["business_name"]},
            {"status", "running"},
            {"components", deployed_components},
            {"created_at", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
        };
    }
    
    // 保存到数据库
    db_manager_->saveBusiness(businesses_[business_id]);
    
    return {
        {"status", "success"},
        {"message", "Business deployed successfully"},
        {"business_id", business_id}
    };
}

nlohmann::json BusinessManager::stopBusiness(const std::string& business_id) {
    std::cout << "Stopping business: " << business_id << std::endl;
    
    // 获取业务信息
    auto business = db_manager_-> getBusinessDetails(business_id);

    // auto business = businesses_[business_id];
    // 打印业务信息
    std::cout << "Business info: " << business.dump() << std::endl;
    
    // 停止所有组件
    for (const auto& component_item : business["components"]) {
        std::string component_id = component_item["component_id"];
        
        // 停止组件
        // 打印停止组件信息
        std::cout << "Stopping component: " << component_id << ", business_id: " << business_id << std::endl;

        auto stop_result = stopComponent(business_id, component_id);
        
        if (stop_result["status"] != "success") {
            std::cerr << "Failed to stop component: " << component_id << std::endl;
        }
    }
    
    // 更新业务状态
    {
        std::lock_guard<std::mutex> lock(businesses_mutex_);
        
        businesses_[business_id]["status"] = "stopped";
        businesses_[business_id]["stopped_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    
    // 更新数据库
    db_manager_->saveBusiness(businesses_[business_id]);
    
    return {
        {"status", "success"},
        {"message", "Business stopped successfully"}
    };
}

nlohmann::json BusinessManager::restartBusiness(const std::string& business_id) {
    std::cout << "Restarting business: " << business_id << std::endl;
    
    // 先停止业务
    auto stop_result = stopBusiness(business_id);
    
    if (stop_result["status"] != "success") {
        return stop_result;
    }
    
    // 获取业务信息
    // auto business = db_manager_->getBusiness(business_id); // TODO: 需实现或通过其他方式获取业务信息
    
    if (businesses_.empty()) {
        return {
            {"status", "error"},
            {"message", "Business not found"}
        };
    }
    
    // 重新部署业务
    return deployBusiness(businesses_.begin()->second);
}

nlohmann::json BusinessManager::updateBusiness(const std::string& business_id, const nlohmann::json& business_info) {
    std::cout << "Updating business: " << business_id << std::endl;
    
    // 验证业务信息
    if (!validateBusinessInfo(business_info)) {
        return {
            {"status", "error"},
            {"message", "Invalid business information"}
        };
    }
    
    // 先停止业务
    auto stop_result = stopBusiness(business_id);
    
    if (stop_result["status"] != "success") {
        return stop_result;
    }
    
    // 更新业务信息
    nlohmann::json updated_business = business_info;
    updated_business["business_id"] = business_id;
    
    // 重新部署业务
    return deployBusiness(updated_business);
}

nlohmann::json BusinessManager::getBusinesses() {
    // 从数据库获取所有业务
    auto businesses = db_manager_->getBusinesses();
    
    // 更新内存缓存
    {
        std::lock_guard<std::mutex> lock(businesses_mutex_);
        
        for (const auto& business : businesses) {
            businesses_[business["business_id"]] = business;
        }
    }
    
    return {
        {"status", "success"},
        {"businesses", businesses}
    };
}

nlohmann::json BusinessManager::getBusinessDetails(const std::string& business_id) {
    // 检查业务是否存在
    // {
    //     std::lock_guard<std::mutex> lock(businesses_mutex_);
        
    //     if (businesses_.find(business_id) == businesses_.end()) {
    //         // 尝试从数据库加载
    //         // auto business = db_manager_->getBusiness(business_id); // TODO: 需实现或通过其他方式获取业务信息
            
    //         if (businesses_.empty()) {
    //             return {
    //                 {"status", "error"},
    //                 {"message", "Business not found"}
    //             };
    //         }
            
    //         businesses_[business_id] = businesses_.begin()->second;
    //     }
    // }

    auto business = db_manager_->getBusinessDetails(business_id);
    
    return {
        {"status", "success"},
        {"business", business}
    };
}

nlohmann::json BusinessManager::getBusinessComponents(const std::string& business_id) {
    // 获取业务详情
    auto business_result = getBusinessDetails(business_id);
    
    if (business_result["status"] != "success") {
        return business_result;
    }
    
    // 获取组件列表
    auto business = business_result["business"];
    
    if (!business.contains("components")) {
        return {
            {"status", "success"},
            {"components", nlohmann::json::array()}
        };
    }
    
    // 获取组件详情
    nlohmann::json components = nlohmann::json::array();
    
    for (const auto& component : business["components"]) {
        auto component_details = db_manager_->getBusinessComponents(business_id);
        nlohmann::json component_item;
        for (const auto& c : component_details["components"]) {
            if (c["component_id"] == component["component_id"]) {
                component_item = c;
                break;
            }
        }
        if (component_item.empty()) {
            return {
                {"status", "error"},
                {"message", "Component not found"}
            };
        }
        components.push_back(component_item);
    }
    
    return {
        {"status", "success"},
        {"components", components}
    };
}

nlohmann::json BusinessManager::handleComponentStatusReport(const nlohmann::json& component_status) {
    // 检查必要字段
    if (!component_status.contains("component_id") || 
        !component_status.contains("business_id") || 
        !component_status.contains("status")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    std::string component_id = component_status["component_id"];
    std::string business_id = component_status["business_id"];
    std::string status = component_status["status"];
    
    // 保存组件状态
    db_manager_->updateComponentStatus(component_id, status, component_status.value("container_id", ""));
    
    // 更新业务状态
    {
        std::lock_guard<std::mutex> lock(businesses_mutex_);
        
        if (businesses_.find(business_id) != businesses_.end()) {
            auto& business = businesses_[business_id];
            
            if (business.contains("components") && business["components"].is_array()) {
                for (auto& component : business["components"]) {
                    if (component["component_id"] == component_id) {
                        component["status"] = status;
                        break;
                    }
                }
            }
            
            // 更新数据库
            db_manager_->saveBusiness(business);
        }
    }
    
    return {
        {"status", "success"},
        {"message", "Component status updated"}
    };
}

bool BusinessManager::validateBusinessInfo(const nlohmann::json& business_info) {
    // 检查必要字段
    if (!business_info.contains("business_name")) {
        return false;
    }
    
    // 检查组件
    if (!business_info.contains("components") || !business_info["components"].is_array()) {
        return false;
    }
    
    // 验证每个组件
    for (const auto& component : business_info["components"]) {
        if (!validateComponentInfo(component)) {
            return false;
        }
    }
    
    return true;
}

bool BusinessManager::validateComponentInfo(const nlohmann::json& component_info) {
    // 检查必要字段
    if (!component_info.contains("component_id") || 
        !component_info.contains("component_name") || 
        !component_info.contains("type")) {
        return false;
    }
    
    std::string type = component_info["type"];
    
    // 根据类型检查特定字段
    if (type == "docker") {
        // 检查Docker特定字段
        if ((!component_info.contains("image_url") || component_info["image_url"].get<std::string>().empty()) && 
            (!component_info.contains("image_name") || component_info["image_name"].get<std::string>().empty())) {
            return false;
        }
    } else if (type == "binary") {
        // 检查二进制特定字段
        if (!component_info.contains("binary_path") && !component_info.contains("binary_url")) {
            return false;
        }
    } else {
        // 不支持的类型
        return false;
    }
    
    return true;
}

nlohmann::json BusinessManager::deployComponent(const std::string& business_id, 
                                             const nlohmann::json& component_info, 
                                             const std::string& node_id) {
    // 获取节点信息
    auto nodes = db_manager_->getAgents();
    
    // 查找指定的节点
    nlohmann::json node_info;
    bool node_found = false;
    
    for (const auto& node : nodes) {
        std::string current_node_id = node.contains("node_id") ? node["node_id"].get<std::string>() : node["agent_id"].get<std::string>();
        if (current_node_id == node_id) {
            node_info = node;
            node_found = true;
            break;
        }
    }
    
    if (!node_found) {
        return {
            {"status", "error"},
            {"message", "Node not found"}
        };
    }
    
    // 获取节点URL
    std::string node_url;
    if (node_info.contains("ip_address")) {
        // 假设Agent的IP地址可以用作URL的一部分
        node_url = "http://" + node_info["ip_address"].get<std::string>() + ":8081";
    } else {
        return {
            {"status", "error"},
            {"message", "Node URL not found"}
        };
    }
    
    // 准备部署请求
    nlohmann::json deploy_request = component_info;
    deploy_request["business_id"] = business_id;
    
    // 发送部署请求
    nlohmann::json response;
    try {
        // 解析URL
        std::string url = node_url;
        std::string host;
        std::string path = "/api/deploy";
        int port = 8081;
        
        // 从node_url中提取host和port
        if (url.substr(0, 7) == "http://") {
            url = url.substr(7);
        }
        
        size_t pos = url.find(':');
        if (pos != std::string::npos) {
            host = url.substr(0, pos);
            port = std::stoi(url.substr(pos + 1));
        } else {
            pos = url.find('/');
            if (pos != std::string::npos) {
                host = url.substr(0, pos);
                path = url.substr(pos) + path;
            } else {
                host = url;
            }
        }
        
        // 创建HTTP客户端
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);  // 5秒超时
        cli.set_read_timeout(5);
        
        // 设置请求头
        httplib::Headers header_map;
        header_map.emplace("Content-Type", "application/json");
        
        // 将JSON数据转换为字符串
        std::string json_data = deploy_request.dump();
        
        // 发送POST请求
        auto res = cli.Post(path, header_map, json_data, "application/json");
        
        // 处理响应
        if (res && res->status == 200) {
            try {
                response = nlohmann::json::parse(res->body);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
                response = nlohmann::json({{"status", "error"}, {"message", "Invalid JSON response"}});
            }
        } else {
            std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
            response = nlohmann::json({{"status", "error"}, {"message", error_msg}});
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in deployComponent: " << e.what() << std::endl;
        response = nlohmann::json({{"status", "error"}, {"message", std::string("Exception: ") + e.what()}});
    }
    
    // 保存组件信息
    if (response["status"] == "success") {
        // 添加节点信息
        nlohmann::json component = component_info;
        component["node_id"] = node_id;
        component["business_id"] = business_id;
        component["status"] = "running";
        
        // 添加容器ID或进程ID
        if (component["type"] == "docker" && deploy_request.contains("container_id")) {
            component["container_id"] = deploy_request["container_id"];
        } else if (component["type"] == "binary" && deploy_request.contains("process_id")) {
            component["process_id"] = deploy_request["process_id"];
        }
        
        // 打印组件信息
        std::cout << "save Component info: " << component.dump() << std::endl;

        // 保存到数据库
        db_manager_->saveBusinessComponent(component);
    }
    
    return response;
}

nlohmann::json BusinessManager::stopComponent(const std::string& business_id, const std::string& component_id) {

    // 打印停止组件信息
    std::cout << "Stopping component: " << component_id << ", business_id: " << business_id << std::endl;

    // 获取组件信息
    auto components = db_manager_->getBusinessComponents(business_id);

    nlohmann::json component;
    for (const auto& c : components) {
        if (c["component_id"] == component_id) {
            component = c;
            break;
        }
    }
    if (component.empty()) {
        return {
            {"status", "error"},
            {"message", "Component not found"}
        };
    }
    
    // 获取节点信息
    std::string node_id = component["node_id"].get<std::string>();
    auto nodes = db_manager_->getAgents();
    
    // 查找指定的节点
    nlohmann::json node_info;
    bool node_found = false;
    
    for (const auto& node : nodes) {
        std::string current_node_id = node.contains("node_id") ? node["node_id"].get<std::string>() : node["agent_id"].get<std::string>();
        if (current_node_id == node_id) {
            node_info = node;
            node_found = true;
            break;
        }
    }
    
    if (!node_found) {
        return {
            {"status", "error"},
            {"message", "Node not found"}
        };
    }
    
    // 获取节点URL
    std::string node_url;
    if (node_info.contains("ip_address")) {
        // 假设Agent的IP地址可以用作URL的一部分
        node_url = "http://" + node_info["ip_address"].get<std::string>() + ":8081";
    } else {
        return {
            {"status", "error"},
            {"message", "Node URL not found"}
        };
    }
    
    // 准备停止请求
    nlohmann::json stop_request = {
        {"component_id", component_id},
        {"business_id", business_id}
    };
    
    // 添加容器ID或进程ID
    if (component["type"] == "docker" && component.contains("container_id")) {
        stop_request["container_id"] = component["container_id"];
        stop_request["component_type"] = "docker";
    } else if (component["type"] == "binary" && component.contains("process_id")) {
        stop_request["process_id"] = component["process_id"];
        stop_request["component_type"] = "binary";
    }
        
    // 打印停止请求
    std::cout << "Stop request: " << stop_request.dump() << std::endl;

    // 发送停止请求
    nlohmann::json response;
    try {
        // 解析URL
        std::string url = node_url;
        std::string host;
        std::string path = "/api/stop";
        int port = 8081;
        
        // 从node_url中提取host和port
        if (url.substr(0, 7) == "http://") {
            url = url.substr(7);
        }
        
        size_t pos = url.find(':');
        if (pos != std::string::npos) {
            host = url.substr(0, pos);
            port = std::stoi(url.substr(pos + 1));
        } else {
            pos = url.find('/');
            if (pos != std::string::npos) {
                host = url.substr(0, pos);
                path = url.substr(pos) + path;
            } else {
                host = url;
            }
        }
        
        // 创建HTTP客户端
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);  // 5秒超时
        cli.set_read_timeout(5);
        
        // 设置请求头
        httplib::Headers header_map;
        header_map.emplace("Content-Type", "application/json");
        
        // 将JSON数据转换为字符串
        std::string json_data = stop_request.dump();
        
        // 发送POST请求
        auto res = cli.Post(path, header_map, json_data, "application/json");
        
        // 处理响应
        if (res && res->status == 200) {
            try {
                response = nlohmann::json::parse(res->body);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
                response = nlohmann::json({{"status", "error"}, {"message", "Invalid JSON response"}});
            }
        } else {
            std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
            response = nlohmann::json({{"status", "error"}, {"message", error_msg}});
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in stopComponent: " << e.what() << std::endl;
        response = nlohmann::json({{"status", "error"}, {"message", std::string("Exception: ") + e.what()}});
    }
    
    // 更新组件状态
    if (response["status"] == "success") {
        component["status"] = "stopped";
        db_manager_->saveBusinessComponent(component);
    }
    
    return response;
}
