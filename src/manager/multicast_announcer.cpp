#include "multicast_announcer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <nlohmann/json.hpp>

// 构造函数，初始化多播参数
MulticastAnnouncer::MulticastAnnouncer(int port, const std::string& multicast_addr, int multicast_port, int interval_sec)
    : port_(port), multicast_addr_(multicast_addr), multicast_port_(multicast_port), interval_sec_(interval_sec), running_(false) {}

// 析构函数，自动停止多播线程
MulticastAnnouncer::~MulticastAnnouncer() {
    stop();
}

// 启动多播线程
void MulticastAnnouncer::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&MulticastAnnouncer::run, this);
}

// 停止多播线程
void MulticastAnnouncer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

// 多播主循环，定期广播本机IP和端口
void MulticastAnnouncer::run() {
    // 创建UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket for multicast" << std::endl;
        return;
    }
    // 设置多播TTL
    int ttl = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(multicast_addr_.c_str());
    addr.sin_port = htons(multicast_port_);

    std::string local_ip = getLocalIp();
    int counter = 0; // 计数器，用于控制/resource消息的发送频率
    while (running_) {
        // 构造/heartbeat多播消息
        nlohmann::json data_json = {
            {"manager_ip", local_ip},
            {"manager_port", port_},
            {"url", "/heartbeat"} // 默认使用/heartbeat作为URL
        };
        
        nlohmann::json msg_json = {
            {"api_version", 1},
            {"data", data_json}
        };
        std::string msg = msg_json.dump();
        // 发送多播消息
        int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            std::cerr << "Failed to send multicast: " << strerror(errno) << std::endl;
        }
        
        // 每隔3次发送/resource消息
        if (counter % 3 == 0) {
            // 构造/resource多播消息
            nlohmann::json resource_data_json = {
                {"manager_ip", local_ip},
                {"manager_port", port_},
                {"url", "/resource"} // 使用/resource作为URL
            };
            
            nlohmann::json resource_msg_json = {
                {"api_version", 1},
                {"data", resource_data_json}
            };
            std::string resource_msg = resource_msg_json.dump();
            // 发送多播消息
            int resource_ret = sendto(sock, resource_msg.c_str(), resource_msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
            if (resource_ret < 0) {
                std::cerr << "Failed to send resource multicast: " << strerror(errno) << std::endl;
            }
        }
        
        counter++;
        // 间隔等待
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
    }
    close(sock);
}

// 获取本机局域网IP地址
std::string MulticastAnnouncer::getLocalIp() {
    char host[256] = {0};
    if (gethostname(host, sizeof(host)) == 0) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res) {
            sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);
            freeaddrinfo(res);
            return ip;
        }
    }
    return "127.0.0.1";
}