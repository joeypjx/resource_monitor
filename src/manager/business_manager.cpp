#include "business_manager.h"
#include "database_manager.h"
#include "scheduler.h"
#include <iostream>
#include <chrono>
#include <uuid/uuid.h>
#include <httplib.h>

BusinessManager::BusinessManager(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {
}

bool BusinessManager::initialize() {
    // 初始化数据库表
    if (!db_manager_->initializeBusinessTables()) {
        std::cerr << "Failed to initialize business tables" << std::endl;
        return false;
    }
    
    // 创建调度器
    scheduler_ = std::make_unique<Scheduler>(db_manager_);
    
    return true;
}

nlohmann::json BusinessManager::deployBusiness(const nlohmann::json& business_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 验证业务信息
    if (!validateBusinessInfo(business_info)) {
        return {
            {"status", "error"},
            {"message", "Invalid business information"}
        };
    }
    
    // 生成业务ID（如果没有提供）
    nlohmann::json business = business_info;
    if (!business.contains("business_id") || business["business_id"].get<std::string>().empty()) {
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse_lower(uuid, uuid_str);
        business["business_id"] = std::string(uuid_str);
    }
    
    // 设置业务状态为pending
    business["status"] = "pending";
    
    // 保存业务信息到数据库
    if (!db_manager_->saveBusiness(business)) {
        return {
            {"status", "error"},
            {"message", "Failed to save business information"}
        };
    }
    
    // 调度业务组件
    auto schedule_result = scheduleBusinessComponents(business);
    
    // 检查是否有调度失败的组件
    if (schedule_result["failed"].size() > 0) {
        // 更新业务状态为failed
        db_manager_->updateBusinessStatus(business["business_id"], "failed");
        
        return {
            {"status", "error"},
            {"message", "Failed to schedule some components"},
            {"business_id", business["business_id"]},
            {"failed_components", schedule_result["failed"]}
        };
    }
    
    // 更新业务状态为deploying
    db_manager_->updateBusinessStatus(business["business_id"], "deploying");
    
    // 部署每个组件到指定节点
    bool all_deployed = true;
    nlohmann::json failed_components = nlohmann::json::array();
    
    for (const auto& component_schedule : schedule_result["scheduled"]) {
        std::string component_id = component_schedule["component_id"];
        std::string node_id = component_schedule["node_id"];
        
        // 查找组件信息
        nlohmann::json component_info;
        for (const auto& component : business["components"]) {
            if (component["component_id"] == component_id) {
                component_info = component;
                break;
            }
        }
        
        // 添加业务ID到组件信息
        component_info["business_id"] = business["business_id"];
        
        // 设置组件状态为pending
        component_info["status"] = "pending";
        component_info["node_id"] = node_id;
        
        // 保存组件信息到数据库
        if (!db_manager_->saveBusinessComponent(component_info)) {
            all_deployed = false;
            nlohmann::json failed_component;
            failed_component["component_id"] = component_id;
            failed_component["reason"] = "Failed to save component information";
            failed_components.push_back(failed_component);
            continue;
        }
        
        // 部署组件到指定节点
        auto deploy_result = deployComponentToNode(component_info, node_id);
        
        if (deploy_result["status"] != "success") {
            all_deployed = false;
            nlohmann::json failed_component;
            failed_component["component_id"] = component_id;
            failed_component["reason"] = deploy_result["message"];
            failed_components.push_back(failed_component);
            
            // 更新组件状态为failed
            db_manager_->updateComponentStatus(component_id, "failed", "");
        } else {
            // 更新组件状态为starting
            std::string container_id = deploy_result.contains("container_id") ? 
                deploy_result["container_id"].get<std::string>() : "";
            db_manager_->updateComponentStatus(component_id, "starting", container_id);
        }
    }
    
    // 更新业务状态
    if (!all_deployed) {
        db_manager_->updateBusinessStatus(business["business_id"], "partially_deployed");
        
        return {
            {"status", "warning"},
            {"message", "Some components failed to deploy"},
            {"business_id", business["business_id"]},
            {"failed_components", failed_components}
        };
    } else {
        db_manager_->updateBusinessStatus(business["business_id"], "running");
        
        return {
            {"status", "success"},
            {"message", "Business deployed successfully"},
            {"business_id", business["business_id"]}
        };
    }
}

nlohmann::json BusinessManager::stopBusiness(const std::string& business_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取业务详情
    auto business = db_manager_->getBusinessDetails(business_id);
    
    if (business.empty()) {
        return {
            {"status", "error"},
            {"message", "Business not found"}
        };
    }
    
    // 更新业务状态为stopping
    db_manager_->updateBusinessStatus(business_id, "stopping");
    
    // 停止每个组件
    bool all_stopped = true;
    nlohmann::json failed_components = nlohmann::json::array();
    
    for (const auto& component : business["components"]) {
        std::string component_id = component["component_id"];
        std::string node_id = component["node_id"];
        std::string container_id = component["container_id"];
        
        // 如果组件没有部署或已经停止，跳过
        if (component["status"] == "pending" || component["status"] == "failed" || 
            component["status"] == "stopped") {
            continue;
        }
        
        // 构造停止请求
        nlohmann::json stop_request = {
            {"component_id", component_id},
            {"business_id", business_id},
            {"container_id", container_id}
        };
        
        // 发送停止请求到节点
        httplib::Client cli("http://" + node_id);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(5);
        
        auto res = cli.Post("/api/stop", stop_request.dump(), "application/json");
        
        if (!res || res->status != 200) {
            all_stopped = false;
            nlohmann::json failed_component;
            failed_component["component_id"] = component_id;
            failed_component["reason"] = "Failed to stop component";
            failed_components.push_back(failed_component);
        } else {
            // 解析响应
            try {
                auto response = nlohmann::json::parse(res->body);
                
                if (response["status"] != "success") {
                    all_stopped = false;
                    nlohmann::json failed_component;
                    failed_component["component_id"] = component_id;
                    failed_component["reason"] = response["message"];
                    failed_components.push_back(failed_component);
                } else {
                    // 更新组件状态为stopped
                    db_manager_->updateComponentStatus(component_id, "stopped", "");
                }
            } catch (const std::exception& e) {
                all_stopped = false;
                nlohmann::json failed_component;
                failed_component["component_id"] = component_id;
                failed_component["reason"] = "Invalid response from node";
                failed_components.push_back(failed_component);
            }
        }
    }
    
    // 更新业务状态
    if (!all_stopped) {
        db_manager_->updateBusinessStatus(business_id, "partially_stopped");
        
        return {
            {"status", "warning"},
            {"message", "Some components failed to stop"},
            {"business_id", business_id},
            {"failed_components", failed_components}
        };
    } else {
        db_manager_->updateBusinessStatus(business_id, "stopped");
        
        return {
            {"status", "success"},
            {"message", "Business stopped successfully"},
            {"business_id", business_id}
        };
    }
}

nlohmann::json BusinessManager::restartBusiness(const std::string& business_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取业务详情
    auto business = db_manager_->getBusinessDetails(business_id);
    
    if (business.empty()) {
        return {
            {"status", "error"},
            {"message", "Business not found"}
        };
    }
    
    // 先停止业务
    auto stop_result = stopBusiness(business_id);
    
    if (stop_result["status"] == "error") {
        return stop_result;
    }
    
    // 重新部署业务
    return deployBusiness(business);
}

nlohmann::json BusinessManager::updateBusiness(const std::string& business_id, const nlohmann::json& business_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取业务详情
    auto current_business = db_manager_->getBusinessDetails(business_id);
    
    if (current_business.empty()) {
        return {
            {"status", "error"},
            {"message", "Business not found"}
        };
    }
    
    // 验证业务信息
    if (!validateBusinessInfo(business_info)) {
        return {
            {"status", "error"},
            {"message", "Invalid business information"}
        };
    }
    
    // 合并业务信息
    nlohmann::json updated_business = business_info;
    updated_business["business_id"] = business_id;
    
    // 先停止业务
    auto stop_result = stopBusiness(business_id);
    
    if (stop_result["status"] == "error") {
        return stop_result;
    }
    
    // 重新部署业务
    return deployBusiness(updated_business);
}

nlohmann::json BusinessManager::getBusinesses() {
    return db_manager_->getBusinesses();
}

nlohmann::json BusinessManager::getBusinessDetails(const std::string& business_id) {
    return db_manager_->getBusinessDetails(business_id);
}

nlohmann::json BusinessManager::getBusinessComponents(const std::string& business_id) {
    return db_manager_->getBusinessComponents(business_id);
}

bool BusinessManager::updateComponentStatus(const nlohmann::json& component_status) {
    // 检查必要字段
    if (!component_status.contains("component_id") || !component_status.contains("status")) {
        return false;
    }
    
    std::string component_id = component_status["component_id"];
    std::string status = component_status["status"];
    std::string container_id = component_status.contains("container_id") ? 
        component_status["container_id"].get<std::string>() : "";
    
    // 更新组件状态
    if (!db_manager_->updateComponentStatus(component_id, status, container_id)) {
        return false;
    }
    
    // 如果有资源使用指标，保存到数据库
    if (component_status.contains("resource_usage") && component_status.contains("timestamp")) {
        db_manager_->saveComponentMetrics(
            component_id, 
            component_status["timestamp"].get<long long>(), 
            component_status["resource_usage"]
        );
    }
    
    // 获取组件所属的业务ID
    auto component_details = db_manager_->getComponentDetails(component_id);
    
    if (!component_details.empty() && component_details.contains("business_id")) {
        std::string business_id = component_details["business_id"];
        
        // 获取业务的所有组件
        auto components = db_manager_->getBusinessComponents(business_id);
        
        // 检查所有组件的状态，更新业务状态
        bool all_running = true;
        bool any_failed = false;
        
        for (const auto& component : components) {
            std::string comp_status = component["status"];
            
            if (comp_status == "failed") {
                any_failed = true;
            } else if (comp_status != "running") {
                all_running = false;
            }
        }
        
        // 更新业务状态
        if (any_failed) {
            db_manager_->updateBusinessStatus(business_id, "partially_running");
        } else if (all_running) {
            db_manager_->updateBusinessStatus(business_id, "running");
        }
    }
    
    return true;
}

bool BusinessManager::validateBusinessInfo(const nlohmann::json& business_info) {
    // 检查业务名称
    if (!business_info.contains("business_name") || !business_info["business_name"].is_string() || 
        business_info["business_name"].get<std::string>().empty()) {
        return false;
    }
    
    // 检查组件列表
    if (!business_info.contains("components") || !business_info["components"].is_array() || 
        business_info["components"].empty()) {
        return false;
    }
    
    // 检查每个组件
    for (const auto& component : business_info["components"]) {
        // 检查组件ID
        if (!component.contains("component_id") || !component["component_id"].is_string() || 
            component["component_id"].get<std::string>().empty()) {
            return false;
        }
        
        // 检查组件名称
        if (!component.contains("component_name") || !component["component_name"].is_string() || 
            component["component_name"].get<std::string>().empty()) {
            return false;
        }
        
        // 检查组件类型
        if (!component.contains("type") || !component["type"].is_string() || 
            component["type"].get<std::string>().empty()) {
            return false;
        }
        
        // 检查Docker组件的必要字段
        if (component["type"] == "docker") {
            if ((!component.contains("image_url") || !component["image_url"].is_string()) && 
                (!component.contains("image_name") || !component["image_name"].is_string())) {
                return false;
            }
        }
        
        // 检查资源需求
        if (!component.contains("resource_requirements") || !component["resource_requirements"].is_object()) {
            return false;
        }
    }
    
    return true;
}

nlohmann::json BusinessManager::scheduleBusinessComponents(const nlohmann::json& business_info) {
    return scheduler_->scheduleComponents(business_info);
}

nlohmann::json BusinessManager::deployComponentToNode(const nlohmann::json& component_info, const std::string& node_id) {
    // 构造部署请求
    nlohmann::json deploy_request = component_info;
    
    // 发送部署请求到节点
    httplib::Client cli("http://" + node_id);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(5);
    
    auto res = cli.Post("/api/deploy", deploy_request.dump(), "application/json");
    
    if (!res || res->status != 200) {
        return {
            {"status", "error"},
            {"message", "Failed to connect to node"}
        };
    }
    
    // 解析响应
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", "Invalid response from node"}
        };
    }
}
