#include "agent.h"
#include "cpu_collector.h"
#include "memory_collector.h"
#include "http_client.h"
#include "component_manager.h"
#include "utils/logger.h"
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
#include <cstdlib>
#include <sstream>
#include <vector>
#include <cstdio>
#include <string>
#include <future>
#include <thread>

Agent::Agent(const std::string &manager_url,
             const std::string &hostname,
             int collection_interval_sec,
             int port,
             const std::string &network_interface)
    : manager_url_(manager_url),
      hostname_(hostname),
      collection_interval_sec_(collection_interval_sec),
      port_(port),
      network_interface_(network_interface),
      running_(false),
      http_server_(nullptr),
      server_running_(false)
{
    init();
}

Agent::~Agent()
{
    stop();

    // 停止HTTP服务器
    if (server_running_ && http_server_)
    {
        http_server_->stop();
        delete http_server_;
        http_server_ = nullptr;
        server_running_ = false;
    }
}

bool Agent::start()
{
    // 如果已经在运行，直接返回
    if (running_)
    {
        return true;
    }

    // 向Manager注册
    if (!registerToManager())
    {
        LOG_ERROR("Failed to register to Manager");
        return false;
    }

    // 初始化组件管理器
    if (!component_manager_->initialize())
    {
        LOG_ERROR("Failed to initialize component manager");
        return false;
    }

    // 启动组件状态收集
    if (!component_manager_->startStatusCollection(collection_interval_sec_))
    {
        LOG_ERROR("Failed to start component status collection");
        return false;
    }

    // 启动HTTP服务器
    if (!startHttpServer(port_))
    {
        LOG_ERROR("Failed to start HTTP server");
        return false;
    }

    // 启动工作线程
    worker_thread_ = std::thread(&Agent::workerThread, this);

    // 设置运行标志
    running_ = true;

    return true;
}

void Agent::stop()
{
    // 如果没有在运行，直接返回
    if (!running_)
    {
        return;
    }

    // 清除运行标志
    running_ = false;

    // 停止组件状态收集
    component_manager_->stopStatusCollection();

    // 等待工作线程结束
    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }

    // 停止HTTP服务器
    if (server_running_ && http_server_)
    {
        http_server_->stop();
        delete http_server_;
        http_server_ = nullptr;
        server_running_ = false;
    }
}

// 注册到Manager
bool Agent::registerToManager()
{
    // 本地agent_id文件路径
    std::string agent_id_file = "agent_id.txt";
    // 优先尝试从本地文件读取agent_id
    if (agent_id_.empty())
    {
        agent_id_ = readAgentIdFromFile(agent_id_file);
    }

    nlohmann::json register_info;
    if (!agent_id_.empty())
    {
        // 已有agent_id，带上注册
        register_info["node_id"] = agent_id_;
    }
    register_info["hostname"] = getHostname();
    register_info["ip_address"] = getLocalIpAddress();
    register_info["os_info"] = getOsInfo();
    register_info["cpu_model"] = getCpuModel();
    register_info["gpu_count"] = getGpuCount();
    register_info["port"] = port_;

    // 发送注册请求
    nlohmann::json response = http_client_->registerAgent(register_info);

    // 检查响应
    if (response.contains("status") && response["status"] == "success")
    {
        // 使用服务器返回的Node ID
        if (response.contains("node_id"))
        {
            agent_id_ = response["node_id"];
            writeAgentIdToFile(agent_id_file, agent_id_);
        }
        LOG_INFO("Successfully registered to Manager with Node ID: {}", agent_id_);

        // 将response中的components保存到component_manager中
        if (response.contains("components"))
        {
            for (const auto &component : response["components"])
            {
                component_manager_->addComponent(component);
            }
        }

        return true;
    }
    else
    {
        LOG_ERROR("Failed to register to Manager: {}", response.contains("message") ? response["message"].get<std::string>() : "Unknown error");
        return false;
    }
}

// 采集并上报资源信息
void Agent::collectAndReportResources()
{
    nlohmann::json report_json;
    report_json["node_id"] = agent_id_;
    report_json["timestamp"] = std::time(nullptr);

    nlohmann::json resource_json;
    // 采集各类资源信息，按类型放入resource字段
    for (const auto &collector : collectors_)
    {
        resource_json[collector->getType()] = collector->collect();
    }
    report_json["resource"] = resource_json;

    nlohmann::json components = component_manager_->getComponentStatus();
    report_json["components"] = components;

    // 上报资源数据
    nlohmann::json response = http_client_->reportData(report_json);
    // 检查响应
    if (response.contains("status") && response["status"] == "success")
    {
        // LOG_INFO("Successfully reported resource data to Manager: {}", report_json.dump(4));
    }
    else
    {
        LOG_ERROR("Failed to report resource data to Manager: {}", response.contains("message") ? response["message"].get<std::string>() : "Unknown error");
    }
}

void Agent::workerThread()
{
    while (running_)
    {
        // 采集并上报资源信息
        collectAndReportResources();

        // 等待指定的时间间隔
        for (int i = 0; i < collection_interval_sec_ && running_; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// 启动HTTP服务器
bool Agent::startHttpServer(int port)
{
    // 如果服务器已经在运行，直接返回
    if (server_running_)
    {
        return true;
    }

    // 创建HTTP服务器
    auto server = new httplib::Server();
    http_server_ = server;

    // 设置API路由

    // 部署组件API
    server->Post("/api/deploy", [this](const httplib::Request &req, httplib::Response &res)
                 {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleDeployRequest(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            LOG_ERROR("error: {}", e.what());
            res.set_content(nlohmann::json({
                {"status", "error"},
                {"message", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        } });

    // 停止组件API
    server->Post("/api/stop", [this](const httplib::Request &req, httplib::Response &res)
                 {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleStopRequest(request);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(nlohmann::json({
                {"status", "error"},
                {"message", std::string("Invalid request: ") + e.what()}
            }).dump(), "application/json");
        } });

    // 启动服务器
    LOG_INFO("Starting HTTP server on port {}", port);
    server_running_ = true;

    // 在新线程中启动服务器
    std::thread([this, server, port]()
                { server->listen("0.0.0.0", port); })
        .detach();

    return true;
}

nlohmann::json Agent::handleDeployRequest(const nlohmann::json &request)
{
    // 检查必要字段
    if (!request.contains("component_id") || !request.contains("business_id") ||
        !request.contains("component_name") || !request.contains("type"))
    {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}};
    }

    // 异步后台部署
    std::thread([this, request]()
                { component_manager_->deployComponent(request); })
        .detach();

    return {
        {"status", "success"},
        {"message", "Deploy request is being processed asynchronously"}};
}

nlohmann::json Agent::handleStopRequest(const nlohmann::json &request)
{
    // 检查必要字段
    if (!request.contains("component_id") || !request.contains("business_id"))
    {
        return {
            {"status", "error"},
            {"message", "Missing required fields"}};
    }

    // 调用组件管理器停止组件
    // 异步后台执行
    std::thread([this, request]()
                { 
                    component_manager_->stopComponent(request);
                    if (request.contains("permanently") && request["permanently"]) {
                        component_manager_->removeComponent(request["component_id"]);
                    }
                })
        .detach();

    return {
        {"status", "success"},
        {"message", "Stop request is being processed asynchronously"}};
}

void Agent::init() {
    // 创建HTTP客户端
    http_client_ = std::make_shared<HttpClient>(manager_url_);
    // 创建资源采集器
    collectors_.clear();
    collectors_.push_back(std::make_unique<CpuCollector>());
    collectors_.push_back(std::make_unique<MemoryCollector>());
    // 创建组件管理器
    component_manager_ = std::make_shared<ComponentManager>(http_client_);
}

// 获取本地agent_id

std::string Agent::readAgentIdFromFile(const std::string& file_path) {
    std::ifstream fin(file_path);
    std::string id;
    if (fin) {
        std::getline(fin, id);
        fin.close();
    }
    return id;
}

void Agent::writeAgentIdToFile(const std::string& file_path, const std::string& id) {
    std::ofstream fout(file_path);
    if (fout) {
        fout << id << std::endl;
        fout.close();
    }
}

// 获取本地信息

std::string Agent::getHostname() {
    // 如果没有提供主机名，则自动获取
    if (hostname_.empty()) {
        char host[256];
        if (gethostname(host, sizeof(host)) == 0) {
            hostname_ = host;
        } else {
            hostname_ = "unknown";
        }
    }
    return hostname_;
}

std::string Agent::getLocalIpAddress() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    std::string ip;
    
    if (getifaddrs(&ifaddr) == -1) {
        return "127.0.0.1";
    }
    
    // 如果指定了网络接口，优先使用该接口的IP地址
    if (!network_interface_.empty()) {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            family = ifa->ifa_addr->sa_family;
            if (family == AF_INET && strcmp(ifa->ifa_name, network_interface_.c_str()) == 0) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s == 0) {
                    ip = host;
                    break;
                }
            }
        }
    }
    
    // 如果没有找到指定接口的IP地址，或者没有指定接口，则使用原来的逻辑
    if (ip.empty()) {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            family = ifa->ifa_addr->sa_family;
            if (family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s != 0) continue;
                // 跳过lo和docker网络接口
                if (strcmp(ifa->ifa_name, "lo") != 0 && 
                    strncmp(ifa->ifa_name, "docker", 6) != 0) {
                    ip = host;
                    break;
                }
            }
        }
    }
    
    freeifaddrs(ifaddr);
    if (ip.empty()) ip = "127.0.0.1";
    return ip;
}

std::string Agent::getOsInfo() {
    struct utsname system_info;
    if (uname(&system_info) == -1) {
        return "Unknown";
    }
    return std::string(system_info.sysname) + " " +
           std::string(system_info.release) + " " +
           std::string(system_info.version) + " " +
           std::string(system_info.machine);
}

std::string Agent::getCpuModel() {
    FILE *pipe = popen("cat /proc/cpuinfo | grep 'model name' | uniq | awk -F': ' '{print $2}'", "r");
    if (!pipe) return "Unknown";
    char buffer[128];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);
    return result;
}

int Agent::getGpuCount() {
    FILE *pipe = popen("LD_LIBRARY_PATH=/usr/local/corex/lib/ /usr/local/corex/bin/ixsmi -L | grep 'UUID' | wc -l", "r");
    if (!pipe) return 0;
    char buffer[128];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);
    try {
        return std::stoi(result);
    } catch (const std::exception &e) {
        LOG_ERROR("Error getting GPU count: {}", e.what());
        return 0;
    }
}