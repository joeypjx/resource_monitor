#include "component_manager.h"
#include "docker_manager.h"
#include "binary_manager.h"
#include "http_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "dir_utils.h"

ComponentManager::ComponentManager(std::shared_ptr<HttpClient> http_client)
    : http_client_(http_client), running_(false), collection_interval_sec_(5) {
}

ComponentManager::~ComponentManager() {
    stopStatusCollection();
}

bool ComponentManager::initialize() {
    // 创建Docker管理器
    docker_manager_ = std::make_unique<DockerManager>();
    
    // 初始化Docker管理器
    if (!docker_manager_->initialize()) {
        std::cerr << "Failed to initialize Docker manager" << std::endl;
        return false;
    }
    
    // 创建二进制运行体管理器
    binary_manager_ = std::make_unique<BinaryManager>();
    
    // 初始化二进制运行体管理器
    if (!binary_manager_->initialize()) {
        std::cerr << "Failed to initialize Binary manager" << std::endl;
        return false;
    }
    
    // 创建组件目录
    create_directories("/tmp/resource_monitor/components");
    create_directories("/opt/resource_monitor/binaries");
    
    return true;
}

nlohmann::json ComponentManager::deployComponent(const nlohmann::json& component_info) {

    // 打印组件信息
    std::cout << "Deploying component: " << component_info.dump() << std::endl;

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
    
    // 根据组件类型调用不同的部署方法
    if (type == "docker") {
        return deployDockerComponent(component_info);
    } else if (type == "binary") {
        return deployBinaryComponent(component_info);
    } else {
        return {
            {"status", "error"},
            {"message", "Unsupported component type: " + type}
        };
    }
}

nlohmann::json ComponentManager::deployDockerComponent(const nlohmann::json& component_info) {
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    std::string component_name = component_info["component_name"];
    
    // 检查Docker相关字段
    if ((!component_info.contains("image_url") || component_info["image_url"].get<std::string>().empty()) && 
        (!component_info.contains("image_name") || component_info["image_name"].get<std::string>().empty())) {
        return {
            {"status", "error"},
            {"message", "Missing Docker image information"}
        };
    }
    
    // 打印组件信息
    std::cout << "Component info: " << component_info.dump() << std::endl;

    // 获取镜像信息
    std::string image_url = component_info.contains("image_url") ? component_info["image_url"].get<std::string>() : "";
    std::string image_name = component_info.contains("image_name") ? component_info["image_name"].get<std::string>() : "";
    
    // 下载或拉取镜像 todo 需要修改
    auto pull_result = downloadImage(image_url, image_name);

    // 打印镜像信息
    std::cout << "Image url: " << image_url << std::endl;
    std::cout << "Image name: " << image_name << std::endl;
    
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
    
    // 打印容器名称
    std::cout << "Container name: " << container_name << std::endl;

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
    component["type"] = "docker";
    components_[component_id] = component;
    
    return {
        {"status", "success"},
        {"message", "Docker component deployed successfully"},
        {"container_id", container_id}
    };
}

nlohmann::json ComponentManager::deployBinaryComponent(const nlohmann::json& component_info) {
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    std::string component_name = component_info["component_name"];
    
    // 检查二进制路径或URL
    if (!component_info.contains("binary_path") && !component_info.contains("binary_url")) {
        return {
            {"status", "error"},
            {"message", "Missing binary_path or binary_url"}
        };
    }
    
    std::string binary_path;
    
    // 下载二进制文件（如果提供了URL）
    if (component_info.contains("binary_url")) {
        std::string binary_url = component_info["binary_url"];
        
        // 如果没有指定binary_path，则使用默认路径
        if (component_info.contains("binary_path")) {
            binary_path = component_info["binary_path"];
        } else {
            // 从URL中提取文件名
            std::string filename = binary_url.substr(binary_url.find_last_of("/") + 1);
            binary_path = "/opt/resource_monitor/binaries/" + business_id + "/" + component_id + "/" + filename;
        }
        
        auto result = downloadBinary(binary_url, binary_path);
        if (result["status"] != "success") {
            return result;
        }
    } else {
        binary_path = component_info["binary_path"];
    }
    
    // 创建配置文件（如果有）
    if (component_info.contains("config_files") && component_info["config_files"].is_array()) {
        if (!createConfigFiles(component_info["config_files"])) {
            return {
                {"status", "error"},
                {"message", "Failed to create config files"}
            };
        }
    }
    
    // 设置工作目录
    std::string working_dir;
    size_t last_slash = binary_path.find_last_of("/");
    if (last_slash != std::string::npos) {
        working_dir = binary_path.substr(0, last_slash);
    } else {
        working_dir = ".";
    }
    
    // 确保工作目录存在
    create_directories(working_dir);
    
    // 设置命令行参数
    std::vector<std::string> command_args;
    if (component_info.contains("command_args") && component_info["command_args"].is_array()) {
        for (const auto& arg : component_info["command_args"]) {
            command_args.push_back(arg);
        }
    }
    
    // 设置环境变量
    nlohmann::json env_vars = component_info.contains("environment_variables") ? 
        component_info["environment_variables"] : nlohmann::json::object();
    
    // 启动进程
    auto result = binary_manager_->startProcess(binary_path, working_dir, command_args, env_vars);
    if (result["status"] != "success") {
        return result;
    }
    
    // 保存组件信息
    nlohmann::json component = component_info;
    component["process_id"] = result["process_id"];
    component["binary_path"] = binary_path;
    component["working_dir"] = working_dir;
    component["status"] = "running";
    component["type"] = "binary";
    components_[component_id] = component;
    
    return {
        {"status", "success"},
        {"message", "Binary component deployed successfully"},
        {"component_id", component_id},
        {"process_id", result["process_id"]}
    };
}

nlohmann::json ComponentManager::stopComponent(const std::string& component_id, 
                                            const std::string& business_id, 
                                            const std::string& container_or_process_id,
                                            ComponentType component_type) {

    std::lock_guard<std::mutex> lock(components_mutex_);
    
    // 检查组件是否存在
    if (components_.find(component_id) == components_.end()) {
        // 如果组件不在内存中，但提供了ID，尝试停止
        if (!container_or_process_id.empty()) {
            if (component_type == ComponentType::DOCKER) {
                return stopDockerComponent(component_id, business_id, container_or_process_id);
            } else if (component_type == ComponentType::BINARY) {
                return stopBinaryComponent(component_id, business_id, std::stoi(container_or_process_id));
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
    
    // 根据组件类型调用不同的停止方法
    if (component["type"] == "docker") {
        std::string container_id = component.contains("container_id") ? 
            component["container_id"].get<std::string>() : "";
        return stopDockerComponent(component_id, business_id, container_id);
    } else if (component["type"] == "binary") {
        int process_id = component.contains("process_id") ? 
            component["process_id"].get<int>() : 0;
        return stopBinaryComponent(component_id, business_id, process_id);
    } else {
        return {
            {"status", "error"},
            {"message", "Unsupported component type"}
        };
    }
}

nlohmann::json ComponentManager::stopDockerComponent(const std::string& component_id, 
                                                   const std::string& business_id, 
                                                   const std::string& container_id) {

    if (container_id.empty()) {
        return {
            {"status", "error"},
            {"message", "Container ID not found"}
        };
    }
    
    // 停止容器
    auto stop_result = docker_manager_->stopContainer(container_id);
    
    if (stop_result["status"] != "success") {
        return stop_result;
    }
    
    // 删除容器
    docker_manager_->removeContainer(container_id);
    
    // 更新组件状态（如果在内存中）
    auto it = components_.find(component_id);
    if (it != components_.end()) {
        it->second["status"] = "stopped";
        it->second["container_id"] = "";
    }
    
    return {
        {"status", "success"},
        {"message", "Docker component stopped successfully"}
    };
}

nlohmann::json ComponentManager::stopBinaryComponent(const std::string& component_id, 
                                                   const std::string& business_id, 
                                                   int process_id) {
    if (process_id <= 0) {
        return {
            {"status", "error"},
            {"message", "Invalid process ID"}
        };
    }
    
    // 停止进程
    auto stop_result = binary_manager_->stopProcess(process_id);
    
    if (stop_result["status"] != "success") {
        return stop_result;
    }
    
    // 更新组件状态（如果在内存中）
    auto it = components_.find(component_id);
    if (it != components_.end()) {
        it->second["status"] = "stopped";
        it->second["process_id"] = 0;
    }
    
    return {
        {"status", "success"},
        {"message", "Binary component stopped successfully"}
    };
}

bool ComponentManager::collectComponentStatus() {
    std::lock_guard<std::mutex> lock(components_mutex_);
    
    for (auto& it : components_) {
        auto& component_id = it.first;
        auto& component = it.second;
        
        // 根据组件类型收集状态
        if (component["type"] == "docker") {
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
        } else if (component["type"] == "binary") {
            // 如果组件没有进程ID，跳过
            if (!component.contains("process_id") || component["process_id"].get<int>() <= 0) {
                continue;
            }
            
            int process_id = component["process_id"];
            // 获取进程状态
            auto status_result = binary_manager_->getProcessStatus(process_id);
            
            // 更新组件状态
            if (status_result["running"]) {
                component["status"] = "running";
                // 获取进程资源使用情况
                auto stats_result = binary_manager_->getProcessStats(process_id);
                if (stats_result.contains("cpu_percent")) {
                    component["resource_usage"] = {
                        {"cpu_percent", stats_result["cpu_percent"]},
                        {"memory_percent", stats_result["memory_percent"]},
                        {"memory_rss_kb", stats_result["memory_rss_kb"]}
                    };
                }
            } else {
                component["status"] = "stopped";
            }
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
            {"status", component["status"]},
            {"type", component["type"]}
        };
        
        // 根据组件类型添加特定信息
        if (component["type"] == "docker") {
            // 添加容器ID
            if (component.contains("container_id")) {
                report["container_id"] = component["container_id"];
            }
        } else if (component["type"] == "binary") {
            // 添加进程ID
            if (component.contains("process_id")) {
                report["process_id"] = component["process_id"];
            }
            // 添加二进制路径
            if (component.contains("binary_path")) {
                report["binary_path"] = component["binary_path"];
            }
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

nlohmann::json ComponentManager::downloadBinary(const std::string& binary_url, const std::string& binary_path) {
    return binary_manager_->downloadBinary(binary_url, binary_path);
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
