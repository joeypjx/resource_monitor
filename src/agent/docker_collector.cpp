#include "docker_collector.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

DockerCollector::DockerCollector(const std::string& docker_socket) 
    : docker_socket_(docker_socket), docker_available_(false) {
    // 检查Docker服务是否可用
    docker_available_ = isDockerAvailable();
}

nlohmann::json DockerCollector::collect() {
    nlohmann::json result;
    
    // 如果Docker服务不可用，返回空结果
    if (!docker_available_) {
        result["container_count"] = 0;
        result["running_count"] = 0;
        result["paused_count"] = 0;
        result["stopped_count"] = 0;
        result["containers"] = nlohmann::json::array();
        return result;
    }
    
    // 获取容器列表
    nlohmann::json containers = getContainers();
    
    // 统计容器数量
    int container_count = 0;
    int running_count = 0;
    int paused_count = 0;
    int stopped_count = 0;
    
    nlohmann::json container_details = nlohmann::json::array();
    
    // 遍历容器列表，获取详细信息
    for (const auto& container : containers) {
        container_count++;
        
        std::string status = container["State"].get<std::string>();
        if (status == "running") {
            running_count++;
        } else if (status == "paused") {
            paused_count++;
        } else {
            stopped_count++;
        }
        
        // 获取容器ID和名称
        std::string container_id = container["Id"].get<std::string>().substr(0, 12);  // 短ID
        std::string name = container["Names"][0].get<std::string>();
        if (name.at(0) == '/') {
            name = name.substr(1);  // 移除前导斜杠
        }
        
        // 获取镜像名称
        std::string image = container["Image"].get<std::string>();
        
        // 如果容器正在运行，获取资源使用情况
        nlohmann::json container_detail;
        container_detail["id"] = container_id;
        container_detail["name"] = name;
        container_detail["image"] = image;
        container_detail["status"] = status;
        
        if (status == "running") {
            // 获取容器统计信息
            nlohmann::json stats = getContainerStats(container_id);
            
            // 提取CPU和内存使用情况
            if (!stats.is_null()) {
                // 计算CPU使用率
                double cpu_percent = 0.0;
                if (stats.contains("cpu_stats") && stats.contains("precpu_stats")) {
                    auto cpu_stats = stats["cpu_stats"];
                    auto precpu_stats = stats["precpu_stats"];
                    
                    if (cpu_stats.contains("cpu_usage") && precpu_stats.contains("cpu_usage") &&
                        cpu_stats.contains("system_cpu_usage") && precpu_stats.contains("system_cpu_usage")) {
                        
                        unsigned long long cpu_total = cpu_stats["cpu_usage"]["total_usage"].get<unsigned long long>();
                        unsigned long long precpu_total = precpu_stats["cpu_usage"]["total_usage"].get<unsigned long long>();
                        unsigned long long system_usage = cpu_stats["system_cpu_usage"].get<unsigned long long>();
                        unsigned long long pre_system_usage = precpu_stats["system_cpu_usage"].get<unsigned long long>();
                        
                        unsigned long long cpu_delta = cpu_total - precpu_total;
                        unsigned long long system_delta = system_usage - pre_system_usage;
                        
                        if (system_delta > 0 && cpu_delta > 0) {
                            int num_cpus = cpu_stats["online_cpus"].get<int>();
                            cpu_percent = (static_cast<double>(cpu_delta) / system_delta) * num_cpus * 100.0;
                        }
                    }
                }
                
                // 获取内存使用量
                unsigned long long memory_usage = 0;
                if (stats.contains("memory_stats") && stats["memory_stats"].contains("usage")) {
                    memory_usage = stats["memory_stats"]["usage"].get<unsigned long long>();
                }
                
                container_detail["cpu_percent"] = cpu_percent;
                container_detail["memory_usage"] = memory_usage;
            } else {
                container_detail["cpu_percent"] = 0.0;
                container_detail["memory_usage"] = 0;
            }
        } else {
            container_detail["cpu_percent"] = 0.0;
            container_detail["memory_usage"] = 0;
        }
        
        container_details.push_back(container_detail);
    }
    
    // 构建结果
    result["container_count"] = container_count;
    result["running_count"] = running_count;
    result["paused_count"] = paused_count;
    result["stopped_count"] = stopped_count;
    result["containers"] = container_details;
    
    return result;
}

std::string DockerCollector::getType() const {
    return "docker";
}

bool DockerCollector::isDockerAvailable() {
    // 尝试连接Docker套接字
    struct sockaddr_un addr;
    int fd;
    
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return false;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, docker_socket_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        return false;
    }
    
    close(fd);
    return true;
}

nlohmann::json DockerCollector::getContainers() {
    std::string response = sendDockerApiRequest("/containers/json?all=true");
    
    try {
        return nlohmann::json::parse(response);
    } catch (const std::exception& e) {
        return nlohmann::json::array();
    }
}

nlohmann::json DockerCollector::getContainerStats(const std::string& container_id) {
    // 获取容器统计信息（非流式）
    std::string response = sendDockerApiRequest("/containers/" + container_id + "/stats?stream=false");
    
    try {
        return nlohmann::json::parse(response);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

std::string DockerCollector::sendDockerApiRequest(const std::string& endpoint, const std::string& method) {
    // 创建Unix域套接字
    struct sockaddr_un addr;
    int fd;
    
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return "";
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, docker_socket_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        return "";
    }
    
    // 构建HTTP请求
    std::ostringstream request;
    request << method << " " << endpoint << " HTTP/1.1\r\n";
    request << "Host: localhost\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    
    // 发送请求
    std::string request_str = request.str();
    if (write(fd, request_str.c_str(), request_str.length()) == -1) {
        close(fd);
        return "";
    }
    
    // 读取响应
    char buffer[4096];
    std::string response;
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        response += buffer;
    }
    
    close(fd);
    
    // 解析HTTP响应，提取JSON部分
    size_t body_pos = response.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        return response.substr(body_pos + 4);
    }
    
    return "";
}
