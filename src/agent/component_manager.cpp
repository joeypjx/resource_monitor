#include "component_manager.h"
#include "docker_manager.h"
#include "http_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

ComponentManager::ComponentManager(std::shared_ptr<HttpClient> http_client)
    : http_client_(http_client), running_(false), collection_interval_sec_(5) {
}

ComponentManager::~ComponentManager() {
    stopStatusCollection();
}

bool create_directories(const std::string& path) {
    size_t pos = 0;
    do {
        pos = path.find_first_of('/', pos + 1);
        std::string subdir = path.substr(0, pos);
        if (subdir.empty()) continue;
        if (mkdir(subdir.c_str(), 0755) && errno != EEXIST) {
            return false;
        }
    } while (pos != std::string::npos);
    return true;
}

bool ComponentManager::initialize() {
    // 创建Docker管理器
    docker_manager_ = std::make_unique<DockerManager>();
    
    // 初始化Docker管理器
    if (!docker_manager_->initialize()) {
        std::cerr << "Failed to initialize Docker manager" << std::endl;
        return false;
    }
    
    // 创建组件目录
    create_directories("/tmp/resource_monitor/components");
    
    return true;
}

nlohmann::json ComponentManager::deployComponent(const nlohmann::json& component_info) {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    // 检查必要字段
    if (!component_info.contains("component_id") || !component_info.contains("business_id") || 
        !component_info.contains("component_name") || !component_info.contains("type")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    std::string component_name = component_info["component_name"];
    std::string type = component_info["type"];
    
    // 目前只支持Docker类型
    if (type != "docker") {
        return {
            {"status", "error"},
            {"message", "Unsupported component type: " + type}
        };
    }
    
    // 检查Docker相关字段
    if ((!component_info.contains("image_url") || component_info["image_url"].get<std::string>().empty()) && 
        (!component_info.contains("image_name") || component_info["image_name"].get<std::string>().empty())) {
        return {
            {"status", "error"},
            {"message", "Missing Docker image information"}
        };
    }
    
    // 获取镜像信息
    std::string image_url = component_info.contains("image_url") ? component_info["image_url"].get<std::string>() : "";
    std::string image_name = component_info.contains("image_name") ? component_info["image_name"].get<std::string>() : "";
    
    // 下载或拉取镜像
    auto pull_result = downloadImage(image_url, image_name);
    
    if (pull_result["status"] != "success") {
        return pull_result;
    }
    
    // 创建配置文件
    if (component_info.contains("config_files") && component_info["config_files"].is_array()) {
        if (!createConfigFiles(component_info["config_files"])) {
            return {
                {"status", "error"},
                {"message", "Failed to create config files"}
            };
        }
    }
    
    // 准备环境变量
    nlohmann::json env_vars = component_info.contains("environment_variables") ? 
        component_info["environment_variables"] : nlohmann::json::object();
    
    // 准备资源限制
    nlohmann::json resource_limits = component_info.contains("resource_requirements") ? 
        component_info["resource_requirements"] : nlohmann::json::object();
    
    // 准备卷挂载
    std::vector<std::string> volumes;
    
    // 如果有配置文件，添加卷挂载
    if (component_info.contains("config_files") && component_info["config_files"].is_array()) {
        for (const auto& config : component_info["config_files"]) {
            if (config.contains("path")) {
                std::string path = config["path"];
                std::string host_path = "/tmp/resource_monitor/components/" + component_id + path;
                volumes.push_back(host_path + ":" + path);
            }
        }
    }
    
    // 创建容器名称
    std::string container_name = "rm_" + business_id.substr(0, 8) + "_" + component_id.substr(0, 8);
    
    // 创建并启动容器
    auto create_result = docker_manager_->createContainer(
        image_name.empty() ? "loaded_image:latest" : image_name,
        container_name,
        env_vars,
        resource_limits,
        volumes
    );
    
    if (create_result["status"] != "success") {
        return create_result;
    }
    
    // 获取容器ID
    std::string container_id = create_result["container_id"];
    
    // 保存组件信息
    nlohmann::json component = component_info;
    component["container_id"] = container_id;
    component["status"] = "running";
    components_[component_id] = component;
    
    return {
        {"status", "success"},
        {"message", "Component deployed successfully"},
        {"container_id", container_id}
    };
}

nlohmann::json ComponentManager::stopComponent(const std::string& component_id, 
                                            const std::string& business_id, 
                                            const std::string& container_id) {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    // 检查组件是否存在
    if (components_.find(component_id) == components_.end()) {
        // 如果组件不在内存中，但提供了容器ID，尝试停止
        if (!container_id.empty()) {
            auto stop_result = docker_manager_->stopContainer(container_id);
            
            if (stop_result["status"] == "success") {
                // 删除容器
                docker_manager_->removeContainer(container_id);
                
                return {
                    {"status", "success"},
                    {"message", "Component stopped successfully"}
                };
            } else {
                return stop_result;
            }
        }
        
        return {
            {"status", "error"},
            {"message", "Component not found"}
        };
    }
    
    // 获取组件信息
    const auto& component = components_[component_id];
    
    // 检查业务ID是否匹配
    if (component["business_id"] != business_id) {
        return {
            {"status", "error"},
            {"message", "Business ID mismatch"}
        };
    }
    
    // 获取容器ID
    std::string actual_container_id = component.contains("container_id") ? 
        component["container_id"].get<std::string>() : "";
    
    if (actual_container_id.empty()) {
        return {
            {"status", "error"},
            {"message", "Container ID not found"}
        };
    }
    
    // 停止容器
    auto stop_result = docker_manager_->stopContainer(actual_container_id);
    
    if (stop_result["status"] != "success") {
        return stop_result;
    }
    
    // 删除容器
    docker_manager_->removeContainer(actual_container_id);
    
    // 更新组件状态
    components_[component_id]["status"] = "stopped";
    components_[component_id]["container_id"] = "";
    
    return {
        {"status", "success"},
        {"message", "Component stopped successfully"}
    };
}

bool ComponentManager::collectComponentStatus() {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    for (auto& it : components_) {
        auto& component_id = it.first;
        auto& component = it.second;
        // 如果组件没有容器ID，跳过
        if (!component.contains("container_id") || component["container_id"].get<std::string>().empty()) {
            continue;
        }
        std::string container_id = component["container_id"];
        // 获取容器状态
        auto status_result = docker_manager_->getContainerStatus(container_id);
        if (status_result["status"] == "success") {
            std::string container_status = status_result["container_status"];
            // 更新组件状态
            if (container_status == "running") {
                component["status"] = "running";
                // 获取容器资源使用情况
                auto stats_result = docker_manager_->getContainerStats(container_id);
                if (stats_result["status"] == "success") {
                    component["resource_usage"] = stats_result["resource_usage"];
                }
            } else if (container_status == "exited") {
                component["status"] = "stopped";
            } else {
                component["status"] = container_status;
            }
        } else {
            // 容器可能已经被删除
            component["status"] = "unknown";
        }
        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        component["timestamp"] = timestamp;
    }
    return true;
}

bool ComponentManager::reportComponentStatus() {
    std::lock_guard<std::mutex> lock(components_mutex_);
    for (const auto& it : components_) {
        const auto& component_id = it.first;
        const auto& component = it.second;
        // 准备上报数据
        nlohmann::json report = {
            {"component_id", component_id},
            {"business_id", component["business_id"]},
            {"status", component["status"]}
        };
        // 添加容器ID
        if (component.contains("container_id")) {
            report["container_id"] = component["container_id"];
        }
        // 添加资源使用情况
        if (component.contains("resource_usage")) {
            report["resource_usage"] = component["resource_usage"];
        }
        // 添加时间戳
        if (component.contains("timestamp")) {
            report["timestamp"] = component["timestamp"];
        } else {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::system_clock::to_time_t(now);
            report["timestamp"] = timestamp;
        }
        // 发送状态上报
        auto response = http_client_->post("/api/report/component", report);
        if (!response.contains("status") || response["status"] != "success") {
            std::cerr << "Failed to report component status: " << component_id << std::endl;
        }
    }
    return true;
}

bool ComponentManager::startStatusCollection(int interval_sec) {
    if (running_) {
        return true;
    }
    
    collection_interval_sec_ = interval_sec;
    running_ = true;
    
    // 启动状态收集线程
    collection_thread_ = std::make_unique<std::thread>(&ComponentManager::statusCollectionThread, this);
    
    return true;
}

void ComponentManager::stopStatusCollection() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 等待线程结束
    if (collection_thread_ && collection_thread_->joinable()) {
        collection_thread_->join();
    }
}

nlohmann::json ComponentManager::downloadImage(const std::string& image_url, const std::string& image_name) {
    return docker_manager_->pullImage(image_url, image_name);
}

bool ComponentManager::createConfigFiles(const nlohmann::json& config_files) {
    for (const auto& config : config_files) {
        if (!config.contains("path") || !config.contains("content")) {
            continue;
        }
        
        std::string path = config["path"];
        std::string content = config["content"];
        
        // 创建目录
        std::string dir_path = "/tmp/resource_monitor/components/" + path.substr(0, path.find_last_of('/'));
        create_directories(dir_path);
        
        // 创建文件
        std::string file_path = "/tmp/resource_monitor/components/" + path;
        std::ofstream file(file_path);
        
        if (!file.is_open()) {
            std::cerr << "Failed to create config file: " << file_path << std::endl;
            return false;
        }
        
        file << content;
        file.close();
    }
    
    return true;
}

void ComponentManager::statusCollectionThread() {
    while (running_) {
        // 收集组件状态
        collectComponentStatus();
        
        // 上报组件状态
        reportComponentStatus();
        
        // 等待下一次收集
        std::this_thread::sleep_for(std::chrono::seconds(collection_interval_sec_));
    }
}
