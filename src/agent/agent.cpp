#include "agent.h"
#include "cpu_collector.h"
#include "memory_collector.h"
#include "disk_collector.h"
#include "network_collector.h"
#include "docker_collector.h"
#include "http_client.h"
#include "component_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <uuid/uuid.h>
#include <httplib.h>
#include <fstream>

Agent::Agent(const std::string& manager_url, 
             const std::string& hostname,
             int collection_interval_sec)
    : manager_url_(manager_url),
      hostname_(hostname),
      collection_interval_sec_(collection_interval_sec),
      running_(false),
      http_server_(nullptr),
      server_running_(false) {
    
    // 如果没有提供主机名，则自动获取
    if (hostname_.empty()) {
        char host[256];
        if (gethostname(host, sizeof(host)) == 0) {
            hostname_ = host;
        } else {
            hostname_ = "unknown";
        }
    }
    
    // 获取IP地址
    getLocalIpAddress();
    
    // 获取操作系统信息
    getOsInfo();
    
    // 创建HTTP客户端
    http_client_ = std::make_shared<HttpClient>(manager_url_);
    
    // 创建资源采集器
    collectors_.push_back(std::make_unique<CpuCollector>());
    collectors_.push_back(std::make_unique<MemoryCollector>());
    collectors_.push_back(std::make_unique<DiskCollector>());
    collectors_.push_back(std::make_unique<NetworkCollector>());
    collectors_.push_back(std::make_unique<DockerCollector>());
    
    // 创建组件管理器
    component_manager_ = std::make_shared<ComponentManager>(http_client_);
}

Agent::~Agent() {
    stop();
    
    // 停止HTTP服务器
    if (server_running_ && http_server_) {
        http_server_->stop();
        delete http_server_;
        http_server_ = nullptr;
        server_running_ = false;
    }
}

bool Agent::start() {
    // 如果已经在运行，直接返回
    if (running_) {
        return true;
    }
    
    // 向Manager注册
    if (!registerToManager()) {
        std::cerr << "Failed to register to Manager" << std::endl;
        return false;
    }
    
    // 初始化组件管理器
    if (!component_manager_->initialize()) {
        std::cerr << "Failed to initialize component manager" << std::endl;
        return false;
    }
    
    // 启动组件状态收集
    if (!component_manager_->startStatusCollection(collection_interval_sec_)) {
        std::cerr << "Failed to start component status collection" << std::endl;
        return false;
    }
    
    // 启动HTTP服务器
    if (!startHttpServer()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return false;
    }
    
    // 设置运行标志
    running_ = true;
    
    // 启动工作线程
    worker_thread_ = std::thread(&Agent::workerThread, this);
    
    return true;
}

void Agent::stop() {
    // 如果没有在运行，直接返回
    if (!running_) {
        return;
    }
    
    // 清除运行标志
    running_ = false;
    
    // 停止组件状态收集
    component_manager_->stopStatusCollection();
    
    // 等待工作线程结束
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // 停止HTTP服务器
    if (server_running_ && http_server_) {
        http_server_->stop();
        delete http_server_;
        http_server_ = nullptr;
        server_running_ = false;
    }
}

std::string Agent::getAgentId() const {
    return agent_id_;
}

bool Agent::registerToManager() {
    // 本地agent_id文件路径
    std::string agent_id_file = "agent_id.txt";
    // 优先尝试从本地文件读取agent_id
    if (agent_id_.empty()) {
        std::ifstream fin(agent_id_file);
        if (fin) {
            std::getline(fin, agent_id_);
            fin.close();
        }
    }

    nlohmann::json register_info;
    if (!agent_id_.empty()) {
        // 已有agent_id，带上注册
        register_info["agent_id"] = agent_id_;
    }
    register_info["hostname"] = hostname_;
    register_info["ip_address"] = ip_address_;
    register_info["os_info"] = os_info_;

    // 发送注册请求
    nlohmann::json response = http_client_->registerAgent(register_info);

    // 检查响应
    if (response.contains("status") && response["status"] == "success") {
        // 使用服务器返回的Agent ID
        if (response.contains("agent_id")) {
            agent_id_ = response["agent_id"];
            // 写入本地文件
            std::ofstream fout(agent_id_file);
            if (fout) {
                fout << agent_id_ << std::endl;
                fout.close();
            }
        }
        std::cout << "Successfully registered to Manager with Agent ID: " << agent_id_ << std::endl;
        return true;
    } else {
        std::cerr << "Failed to register to Manager: "
                  << (response.contains("message") ? response["message"].get<std::string>() : "Unknown error")
                  << std::endl;
        return false;
    }
}

void Agent::collectAndReportResources() {
    // 构建资源数据
    nlohmann::json resource_data;
    resource_data["agent_id"] = agent_id_;
    resource_data["timestamp"] = std::time(nullptr);
    
    // 采集各类资源信息
    for (const auto& collector : collectors_) {
        resource_data[collector->getType()] = collector->collect();
    }
    
    // 上报资源数据
    nlohmann::json response = http_client_->reportData(resource_data);
    
    // 检查响应
    if (response.contains("status") && response["status"] == "success") {
        std::cout << "Successfully reported resource data to Manager" << std::endl;
    } else {
        std::cerr << "Failed to report resource data to Manager: " 
                  << (response.contains("message") ? response["message"].get<std::string>() : "Unknown error")
                  << std::endl;
    }
}

void Agent::workerThread() {
    while (running_) {
        // 采集并上报资源信息
        collectAndReportResources();
        
        // 等待指定的时间间隔
        for (int i = 0; i < collection_interval_sec_ && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void Agent::getLocalIpAddress() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    
    if (getifaddrs(&ifaddr) == -1) {
        ip_address_ = "127.0.0.1";
        return;
    }
    
    // 遍历所有网络接口
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        
        family = ifa->ifa_addr->sa_family;
        
        // 只考虑IPv4地址
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                           host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (s != 0) {
                continue;
            }
            
            // 忽略回环接口
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                ip_address_ = host;
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    // 如果没有找到有效的IP地址，使用本地回环地址
    if (ip_address_.empty()) {
        ip_address_ = "127.0.0.1";
    }
}

void Agent::getOsInfo() {
    struct utsname system_info;
    if (uname(&system_info) == -1) {
        os_info_ = "Unknown";
        return;
    }
    
    os_info_ = std::string(system_info.sysname) + " " + 
               std::string(system_info.release) + " " + 
               std::string(system_info.version) + " " + 
               std::string(system_info.machine);
}

bool Agent::startHttpServer(int port) {
    // 如果服务器已经在运行，直接返回
    if (server_running_) {
        return true;
    }
    
    // 创建HTTP服务器
    auto server = new httplib::Server();
    http_server_ = server;
    
    // 设置API路由
    
    // 部署组件API
    server->Post("/api/deploy", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleDeployRequest(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({
                {"status", "error"},
                {"message", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        }
    });
    
    // 停止组件API
    server->Post("/api/stop", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleStopRequest(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({
                {"status", "error"},
                {"message", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        }
    });
    
    // 启动服务器
    std::cout << "Starting HTTP server on port " << port << std::endl;
    server_running_ = true;
    
    // 在新线程中启动服务器
    std::thread([this, server, port]() {
        server->listen("0.0.0.0", port);
    }).detach();
    
    return true;
}

nlohmann::json Agent::handleDeployRequest(const nlohmann::json& request) {
    // 检查必要字段
    if (!request.contains("component_id") || !request.contains("business_id") || 
        !request.contains("component_name") || !request.contains("type")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    // 调用组件管理器部署组件
    return component_manager_->deployComponent(request);
}

nlohmann::json Agent::handleStopRequest(const nlohmann::json& request) {
    // 检查必要字段
    if (!request.contains("component_id") || !request.contains("business_id")) {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}
        };
    }
    
    std::string component_id = request["component_id"];
    std::string business_id = request["business_id"];
    std::string container_id = request.contains("container_id") ? 
        request["container_id"].get<std::string>() : "";
    
    // 调用组件管理器停止组件
    return component_manager_->stopComponent(component_id, business_id, container_id);
}
