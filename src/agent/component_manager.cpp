#include "component_manager.h"
#include "docker_manager.h"
#include "binary_manager.h"
#include "http_client.h"
#include "utils/logger.h"
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
    : http_client_(http_client), running_(false), collection_interval_sec_(5)
{
}

ComponentManager::~ComponentManager()
{
    stopStatusCollection();
}

bool ComponentManager::initialize()
{
    // 创建Docker管理器
    docker_manager_ = std::make_unique<DockerManager>();

    // 初始化Docker管理器
    if (!docker_manager_->initialize())
    {
        LOG_ERROR("Failed to initialize Docker manager");
        return false;
    }

    // 创建二进制运行体管理器
    binary_manager_ = std::make_unique<BinaryManager>();

    // 初始化二进制运行体管理器
    if (!binary_manager_->initialize())
    {
        LOG_ERROR("Failed to initialize Binary manager");
        return false;
    }

    // 创建组件目录
    create_directories("/tmp/resource_monitor/components");
    create_directories("/opt/resource_monitor/binaries");

    return true;
}

void ComponentManager::addComponent(const nlohmann::json &component_info)
{
    std::lock_guard<std::mutex> lock(components_mutex_);
    components_[component_info["component_id"]] = component_info;
}

// 部署组件
nlohmann::json ComponentManager::deployComponent(const nlohmann::json &component_info)
{
    // 打印组件信息 格式化
    LOG_INFO("Deploying component: {}", component_info.dump(4));

    // 检查必要字段
    if (!component_info.contains("component_id") || !component_info.contains("business_id") ||
        !component_info.contains("component_name") || !component_info.contains("type"))
    {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}};
    }

    // 根据组件类型调用不同的部署方法
    if (component_info["type"] == "docker")
    {
        auto result = deployDockerComponent(component_info);
        if (result["status"] == "success")
        {
            // 保存组件信息
            std::lock_guard<std::mutex> lock(components_mutex_);
            nlohmann::json component = component_info;
            component["container_id"] = result["container_id"];
            component["status"] = "running";
            component["type"] = "docker";
            components_[component_info["component_id"]] = component;
        }
        return result;
    }
    else if (component_info["type"] == "binary")
    {
        auto result = deployBinaryComponent(component_info);
        if (result["status"] == "success")
        {
            std::lock_guard<std::mutex> lock(components_mutex_);
            nlohmann::json component = component_info;
            component["process_id"] = result["process_id"];
            component["status"] = "running";
            component["type"] = "binary";
            components_[component_info["component_id"]] = component;
        }
        return result;
    }
    else
    {
        return {
            {"status", "error"},
            {"message", "Unsupported component type: " + component_info["type"].get<std::string>()}};
    }
}

nlohmann::json ComponentManager::deployDockerComponent(const nlohmann::json &component_info)
{
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    std::string component_name = component_info["component_name"];

    // 检查Docker相关字段，image_name是必须的，image_url是可选的
    if (!component_info.contains("image_name") || component_info["image_name"].get<std::string>().empty())
    {
        return {
            {"status", "error"},
            {"message", "Missing Docker image name"}};
    }

    // 获取镜像信息
    std::string image_url = component_info.contains("image_url") ? component_info["image_url"].get<std::string>() : "";
    std::string image_name = component_info["image_name"];

    // 下载或拉取镜像
    auto pull_result = docker_manager_->pullImage(image_url, image_name);

    if (pull_result["status"] != "success") // 如果拉取失败，则返回错误信息
    {
        return pull_result;
    }

    // 创建配置文件
    if (component_info.contains("config_files") && component_info["config_files"].is_array())
    {
        if (!createConfigFiles(component_info["config_files"]))
        {
            return {
                {"status", "error"},
                {"message", "Failed to create config files"}};
        }
    }

    // 准备环境变量
    nlohmann::json env_vars = component_info.contains("environment_variables") ? component_info["environment_variables"] : nlohmann::json::object();

    // 准备资源限制
    nlohmann::json resource_limits = component_info.contains("resource_requirements") ? component_info["resource_requirements"] : nlohmann::json::object();

    // 准备卷挂载
    std::vector<std::string> volumes;

    // 如果有配置文件，添加卷挂载
    if (component_info.contains("config_files") && component_info["config_files"].is_array())
    {
        for (const auto &config : component_info["config_files"])
        {
            if (config.contains("path"))
            {
                std::string path = config["path"];
                std::string host_path = "/tmp/resource_monitor/components/" + component_id + path;
                volumes.push_back(host_path + ":" + path);
            }
        }
    }

    // 创建容器名称
    std::string container_name = "c_" + business_id.substr(0, 8) + "_" + component_id.substr(0, 8);

    // 打印容器名称
    std::cout << "Container name: " << container_name << std::endl;

    // 创建并启动容器
    auto create_result = docker_manager_->createContainer(
        image_name,
        container_name,
        env_vars,
        resource_limits,
        volumes);

    if (create_result["status"] != "success")
    {
        return create_result;
    }

    // 获取容器ID
    std::string container_id = create_result["container_id"];

    return {
        {"status", "success"},
        {"message", "Docker component deployed successfully"},
        {"container_id", container_id}};
}

nlohmann::json ComponentManager::deployBinaryComponent(const nlohmann::json &component_info)
{
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    std::string component_name = component_info["component_name"];

    std::string binary_path;
    // 首先确定binary_path
    if (component_info.contains("binary_path") && !component_info["binary_path"].get<std::string>().empty()) {
        binary_path = component_info["binary_path"];
    } else if (component_info.contains("binary_url") && !component_info["binary_url"].get<std::string>().empty()) {
        // 从URL中提取文件名作为默认路径
        std::string binary_url = component_info["binary_url"];
        std::string filename = binary_url.substr(binary_url.find_last_of("/") + 1);
        binary_path = "/opt/resource_monitor/binaries/" + business_id + "/" + component_id + "/" + filename;
    } else {
        return {
            {"status", "error"},
            {"message", "Missing both binary_path and binary_url"}
        };
    }

    // 检查文件是否存在
    std::ifstream file(binary_path);
    if (!file.good()) {
        // 文件不存在,需要下载
        if (!component_info.contains("binary_url") || component_info["binary_url"].get<std::string>().empty()) {
            return {
                {"status", "error"},
                {"message", "Binary file does not exist and no download URL provided"}
            };
        }

        std::string binary_url = component_info["binary_url"];
        auto result = binary_manager_->downloadBinary(binary_url, binary_path);
        if (result["status"] != "success") {
            return result;
        }
    }
    file.close();

    // 创建配置文件（如果有）
    if (component_info.contains("config_files") && component_info["config_files"].is_array())
    {
        if (!createConfigFiles(component_info["config_files"]))
        {
            return {
                {"status", "error"},
                {"message", "Failed to create config files"}};
        }
    }

    // 设置工作目录
    std::string working_dir;
    size_t last_slash = binary_path.find_last_of("/");
    if (last_slash != std::string::npos)
    {
        working_dir = binary_path.substr(0, last_slash);
    }
    else
    {
        working_dir = ".";
    }

    // 确保工作目录存在
    create_directories(working_dir);

    // 设置命令行参数
    std::vector<std::string> command_args;
    if (component_info.contains("command_args") && component_info["command_args"].is_array())
    {
        for (const auto &arg : component_info["command_args"])
        {
            command_args.push_back(arg);
        }
    }

    // 设置环境变量
    nlohmann::json env_vars = component_info.contains("environment_variables") ? component_info["environment_variables"] : nlohmann::json::object();

    // 启动进程
    auto result = binary_manager_->startProcess(binary_path, working_dir, command_args, env_vars);
    if (result["status"] != "success")
    {
        return result;
    }

    return {
        {"status", "success"},
        {"message", "Binary component deployed successfully"},
        {"component_id", component_id},
        {"process_id", result["process_id"]}};
}

nlohmann::json ComponentManager::stopComponent(const nlohmann::json &component_info)
{

    LOG_INFO("Stopping component: {}", component_info.dump(4));
    
    std::string component_id = component_info["component_id"];
    std::string business_id = component_info["business_id"];
    ComponentType component_type = component_info.contains("type") && component_info["type"] == "docker" ? ComponentType::DOCKER : ComponentType::BINARY;
    std::string container_id = component_info.contains("container_id") ? component_info["container_id"].get<std::string>() : "";
    std::string process_id = component_info.contains("process_id") ? component_info["process_id"].get<std::string>() : "";
    std::string container_or_process_id = container_id.empty() ? process_id : container_id;

    if (component_type == ComponentType::DOCKER)
    {
        return stopDockerComponent(component_id, business_id, container_or_process_id);
    }
    else if (component_type == ComponentType::BINARY)
    {
        return stopBinaryComponent(component_id, business_id, container_or_process_id);
    }
    else
    {
        return {
            {"status", "error"},
            {"message", "Unsupported component type:"}};
    }
}

nlohmann::json ComponentManager::stopDockerComponent(const std::string &component_id,
                                                     const std::string &business_id,
                                                     const std::string &container_id)
{

    if (container_id.empty())
    {
        return {
            {"status", "error"},
            {"message", "Container ID not found"}};
    }

    // 停止容器
    auto stop_result = docker_manager_->stopContainer(container_id);

    if (stop_result["status"] != "success")
    {
        return stop_result;
    }

    // 删除容器
    docker_manager_->removeContainer(container_id);

    // 更新组件状态（如果在内存中）
    std::lock_guard<std::mutex> lock(components_mutex_);
    auto it = components_.find(component_id);
    if (it != components_.end())
    {
        it->second["status"] = "stopped";
        it->second["container_id"] = "";
    }

    return {
        {"status", "success"},
        {"message", "Docker component stopped successfully"}};
}

nlohmann::json ComponentManager::stopBinaryComponent(const std::string &component_id,
                                                     const std::string &business_id,
                                                     const std::string &process_id)
{
    if (process_id.empty())
    {
        return {
            {"status", "error"},
            {"message", "Invalid process ID"}};
    }

    // 停止进程
    auto stop_result = binary_manager_->stopProcess(process_id);

    if (stop_result["status"] != "success")
    {
        return stop_result;
    }

    // 更新组件状态（如果在内存中）
    std::lock_guard<std::mutex> lock(components_mutex_);
    auto it = components_.find(component_id);
    if (it != components_.end())
    {
        it->second["status"] = "stopped";
        it->second["process_id"] = "";
    }

    return {
        {"status", "success"},
        {"message", "Binary component stopped successfully"}};
}

// 收集组件状态
bool ComponentManager::collectComponentStatus()
{
    // 先获取所有组件的快照,避免长时间加锁
    std::map<std::string, nlohmann::json> components_snapshot;
    {
        std::lock_guard<std::mutex> lock(components_mutex_);
        components_snapshot = components_;
    }

    // 收集每个组件的状态
    std::map<std::string, nlohmann::json> updated_components;
    for (auto &it : components_snapshot)
    {
        auto &component_id = it.first;
        auto component = it.second;

        // 根据组件类型收集状态
        if (component["type"] == "docker")
        {
            // 如果组件没有容器ID，跳过
            if (!component.contains("container_id") || component["container_id"].get<std::string>().empty())
            {
                continue;
            }

            std::string container_id = component["container_id"];
            // 获取容器状态
            auto status_result = docker_manager_->getContainerStatus(container_id);
            if (status_result["status"] == "success")
            {
                std::string container_status = status_result["container_status"];
                // 更新组件状态
                if (container_status == "running")
                {
                    component["status"] = "running";
                }
                else if (container_status == "exited")
                {
                    component["status"] = "stopped";
                }
                else
                {
                    component["status"] = container_status;
                }
            }
            else
            {
                // 容器可能已经被删除
                component["status"] = "unknown";
            }
        }
        else if (component["type"] == "binary")
        {
            // 如果组件没有进程ID，跳过
            if (!component.contains("process_id") || component["process_id"].get<std::string>().empty())
            {
                continue;
            }

            std::string process_id = component["process_id"];
            // 获取进程状态
            auto status_result = binary_manager_->getProcessStatus(process_id);

            // 更新组件状态
            if (status_result["running"])
            {
                component["status"] = "running";
            }
            else
            {
                component["status"] = "stopped";
            }
        }

        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        component["timestamp"] = timestamp;

        updated_components[component_id] = component;
    }

    // 最后一次性更新所有组件状态
    {
        std::lock_guard<std::mutex> lock(components_mutex_);
        for(const auto& it : updated_components) {
            components_[it.first] = it.second;
        }
    }

    return true;
}

void ComponentManager::statusCollectionThread()
{
    while (running_)
    {
        // 收集所有组件的状态
        collectComponentStatus();

        // 等待指定时间间隔
        std::this_thread::sleep_for(std::chrono::seconds(collection_interval_sec_));
    }
    LOG_INFO("Status collection thread stopped");
}

bool ComponentManager::startStatusCollection(int interval_sec)
{
    if (running_)
    {
        return true;
    }

    collection_interval_sec_ = interval_sec;
    running_ = true;

    // 启动状态收集线程
    collection_thread_ = std::make_unique<std::thread>(&ComponentManager::statusCollectionThread, this);

    return true;
}

void ComponentManager::stopStatusCollection()
{
    if (!running_)
    {
        return;
    }

    running_ = false;

    // 等待线程结束
    if (collection_thread_ && collection_thread_->joinable())
    {
        collection_thread_->join();
    }
}

bool ComponentManager::createConfigFiles(const nlohmann::json &config_files)
{
    for (const auto &config : config_files)
    {
        if (!config.contains("path") || !config.contains("content"))
        {
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

        if (!file.is_open())
        {
            std::cerr << "Failed to create config file: " << file_path << std::endl;
            return false;
        }

        file << content;
        file.close();
    }

    return true;
}

nlohmann::json ComponentManager::getComponentStatus()
{
    std::lock_guard<std::mutex> lock(components_mutex_);

    nlohmann::json result = nlohmann::json::array();
    for (const auto &it : components_)
    {
        result.push_back(it.second);
    }
    return result;
}

bool ComponentManager::removeComponent(const std::string& component_id)
{
    std::lock_guard<std::mutex> lock(components_mutex_);
    auto it = components_.find(component_id);
    if (it != components_.end()) {
        components_.erase(it);
        return true;
    }
    return false;
}