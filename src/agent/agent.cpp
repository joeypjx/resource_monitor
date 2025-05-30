#include "agent.h"
#include "cpu_collector.h"
#include "memory_collector.h"
#include "disk_collector.h"
#include "network_collector.h"
#include "docker_collector.h"
#include "http_client.h"
#include "component_manager.h"
#include "node_controller.h"
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

Agent::Agent(const std::string& manager_url, 
             const std::string& hostname,
             int collection_interval_sec)
    : manager_url_(manager_url),
      hostname_(hostname),
      collection_interval_sec_(collection_interval_sec),
      running_(false),
      http_server_(nullptr),
      server_running_(false),
      heartbeat_running_(false) {
    
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
    node_controller_ = std::make_shared<NodeController>();
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
    
    // 启动心跳线程
    heartbeat_running_ = true;
    heartbeat_thread_ = std::thread(&Agent::heartbeatThread, this);
    
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
    
    // 停止心跳线程
    heartbeat_running_ = false;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
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
        register_info["board_id"] = agent_id_;
    }
    register_info["hostname"] = hostname_;
    register_info["ip_address"] = ip_address_;
    register_info["os_info"] = os_info_;
    register_info["chassis_id"] = getChassisId();
    register_info["cpu_list"] = getCpuList();
    register_info["gpu_list"] = getGpuList();
    
    // 发送注册请求
    nlohmann::json response = http_client_->registerAgent(register_info);
    
    // 检查响应
    if (response.contains("status") && response["status"] == "success") {
        // 使用服务器返回的Board ID
        if (response.contains("board_id")) {
            agent_id_ = response["board_id"];
            // 写入本地文件
            std::ofstream fout(agent_id_file);
            if (fout) {
                fout << agent_id_ << std::endl;
                fout.close();
        }
        }
        std::cout << "Successfully registered to Manager with Board ID: " << agent_id_ << std::endl;
        return true;
    } else {
        std::cerr << "Failed to register to Manager: " 
                  << (response.contains("message") ? response["message"].get<std::string>() : "Unknown error")
                  << std::endl;
        return false;
    }
}

void Agent::collectAndReportResources() {
    nlohmann::json report_json;
    report_json["board_id"] = agent_id_;
    report_json["timestamp"] = std::time(nullptr);
    nlohmann::json resource_json;
    // 采集各类资源信息，按类型放入resource字段
    for (const auto& collector : collectors_) {
        resource_json[collector->getType()] = collector->collect();
    }
    report_json["resource"] = resource_json;
    // 上报资源数据
    nlohmann::json response = http_client_->reportData(report_json);
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

    // 新增：节点控制API
    server->Post("/api/node/control", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto request = nlohmann::json::parse(req.body);
            auto response = handleNodeControlRequest(request);
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

nlohmann::json Agent::handleNodeControlRequest(const nlohmann::json& request) {
    if (!request.contains("action")) {
        return {{"status", "error"}, {"message", "Missing action field"}};
    }
    std::string action = request["action"];
    if (!node_controller_) {
        return {{"status", "error"}, {"message", "NodeController not initialized"}};
    }
    if (action == "shutdown") {
        return node_controller_->shutdown();
    } else if (action == "reboot") {
        return node_controller_->reboot();
    } else {
        return {{"status", "error"}, {"message", "Unknown action: " + action}};
    }
}

void Agent::sendHeartbeat() {
    if (!agent_id_.empty()) {
        nlohmann::json response = http_client_->heartbeat(agent_id_);
        if (response.contains("status") && response["status"] == "success") {
            std::cout << "Heartbeat sent successfully" << std::endl;
        } else {
            std::cerr << "Failed to send heartbeat: "
                      << (response.contains("message") ? response["message"].get<std::string>() : "Unknown error")
                      << std::endl;
        }
    }
}

void Agent::heartbeatThread() {
    while (heartbeat_running_) {
        sendHeartbeat();
        for (int i = 0; i < 3 && heartbeat_running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

std::string Agent::getChassisId() {
    // 尝试从环境变量获取机箱ID
    const char* chassis_id_env = std::getenv("CHASSIS_ID");
    if (chassis_id_env) {
        return std::string(chassis_id_env);
    }
    
    // 尝试从配置文件读取
    std::ifstream chassis_file("/etc/chassis_id");
    if (chassis_file.is_open()) {
        std::string chassis_id;
        std::getline(chassis_file, chassis_id);
        chassis_file.close();
        if (!chassis_id.empty()) {
            return chassis_id;
        }
    }
    
    // 如果都没有，返回默认值
    return "unknown";
}

nlohmann::json Agent::getCpuList() {
    nlohmann::json cpu_list = nlohmann::json::array();
    
    try {
        // 读取/proc/cpuinfo获取CPU信息
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (!cpuinfo.is_open()) {
            return cpu_list;
        }
        
        std::string line;
        nlohmann::json current_cpu;
        bool has_processor = false;
        
        while (std::getline(cpuinfo, line)) {
            if (line.empty()) {
                // 空行表示一个CPU信息结束
                if (has_processor && !current_cpu.empty()) {
                    cpu_list.push_back(current_cpu);
                    current_cpu.clear();
                    has_processor = false;
                }
                continue;
            }
            
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) {
                continue;
            }
            
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "processor") {
                has_processor = true;
                current_cpu["processor_id"] = std::stoi(value);
            } else if (key == "model name") {
                current_cpu["model_name"] = value;
            } else if (key == "cpu MHz") {
                current_cpu["frequency_mhz"] = std::stod(value);
            } else if (key == "cache size") {
                current_cpu["cache_size"] = value;
            } else if (key == "cpu cores") {
                current_cpu["cores"] = std::stoi(value);
            } else if (key == "vendor_id") {
                current_cpu["vendor"] = value;
            }
        }
        
        // 处理最后一个CPU
        if (has_processor && !current_cpu.empty()) {
            cpu_list.push_back(current_cpu);
        }
        
        cpuinfo.close();
    } catch (const std::exception& e) {
        std::cerr << "Error reading CPU information: " << e.what() << std::endl;
    }
    
    return cpu_list;
}

nlohmann::json Agent::getGpuList() {
    nlohmann::json gpu_list = nlohmann::json::array();
    
    try {
        // 尝试使用nvidia-ml-py或nvidia-smi获取GPU信息
        // 这里使用系统命令nvidia-smi作为示例
        FILE* pipe = popen("ixsmi --query-gpu=index,name,memory.total,temperature.gpu,utilization.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string line(buffer);
                // 去除换行符
                if (!line.empty() && line.back() == '\n') {
                    line.pop_back();
                }
                
                // 解析CSV格式的输出
                std::vector<std::string> fields;
                std::stringstream ss(line);
                std::string field;
                
                while (std::getline(ss, field, ',')) {
                    // 去除前后空格
                    field.erase(0, field.find_first_not_of(" \t"));
                    field.erase(field.find_last_not_of(" \t") + 1);
                    fields.push_back(field);
                }
                
                if (fields.size() >= 5) {
                    nlohmann::json gpu;
                    gpu["index"] = std::stoi(fields[0]);
                    gpu["name"] = fields[1];
                    gpu["memory_total_mb"] = std::stoi(fields[2]);
                    gpu["temperature_c"] = std::stoi(fields[3]);
                    gpu["utilization_percent"] = std::stoi(fields[4]);
                    gpu_list.push_back(gpu);
                }
            }
            pclose(pipe);
        }
        
        // 如果nvidia-smi没有找到GPU，尝试查看其他GPU信息
        if (gpu_list.empty()) {
            // 检查是否有其他类型的GPU（Intel、AMD等）
            std::ifstream devices("/proc/bus/pci/devices");
            if (devices.is_open()) {
                std::string line;
                int gpu_index = 0;
                while (std::getline(devices, line)) {
                    // 简化的GPU检测逻辑
                    if (line.find("300") != std::string::npos) { // VGA compatible controller
                        nlohmann::json gpu;
                        gpu["index"] = gpu_index++;
                        gpu["name"] = "Unknown GPU";
                        gpu["type"] = "integrated";
                        gpu_list.push_back(gpu);
                    }
                }
                devices.close();
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error reading GPU information: " << e.what() << std::endl;
    }
    
    return gpu_list;
}
