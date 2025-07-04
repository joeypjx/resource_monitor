#include "http_client.h"
#include <httplib.h>
#include <iostream>

HttpClient::HttpClient(const std::string& base_url) : base_url_(base_url) {
    // 构造函数，初始化基础URL
}

nlohmann::json HttpClient::registerAgent(const nlohmann::json& agent_info) {
    // 发送注册请求
    return post("/api/register", agent_info);
}

nlohmann::json HttpClient::reportData(const nlohmann::json& resource_data) {
    // 上报资源数据
    return post("/api/report", resource_data);
}

nlohmann::json HttpClient::get(const std::string& endpoint, 
                             const std::map<std::string, std::string>& headers) {
    // 解析URL
    std::string url = base_url_;
    std::string host = "";
    std::string path = endpoint;
    int port = 8080;
    
    // 从base_url中提取host和port
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
    httplib::Headers header_map = {};
    for (const auto& header : headers) {
        header_map.emplace(header.first, header.second);
    }
    
    // 发送GET请求
    auto res = cli.Get(path, header_map);
    if ((res) && (res->status == 200)) {
        try {
            return nlohmann::json::parse(res->body);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            return nlohmann::json({{"status", "error"}, {"message", "Invalid JSON response"}});
        }
    } else {
        std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
        return nlohmann::json({{"status", "error"}, {"message", error_msg}});
    }
}

nlohmann::json HttpClient::post(const std::string& endpoint, 
                              const nlohmann::json& data,
                              const std::map<std::string, std::string>& headers) {
    // 解析URL
    std::string url = base_url_;
    std::string host = "";
    std::string path = endpoint;
    int port = 80;
    
    // 从base_url中提取host和port
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
    httplib::Headers header_map = {};
    header_map.emplace("Content-Type", "application/json");
    for (const auto& header : headers) {
        header_map.emplace(header.first, header.second);
    }
    
    // 将JSON数据转换为字符串
    std::string json_data = data.dump();
    
    // 发送POST请求
    auto res = cli.Post(path, header_map, json_data, "application/json");
    if ((res) && (res->status == 200)) {
        try {
            return nlohmann::json::parse(res->body);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            return nlohmann::json({{"status", "error"}, {"message", "Invalid JSON response"}});
        }
    } else {
        std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
        return nlohmann::json({{"status", "error"}, {"message", error_msg}});
    }
}

nlohmann::json HttpClient::heartbeat(const std::string& node_id) {
    std::string endpoint = "/api/heartbeat/" + node_id;
    return post(endpoint, nlohmann::json::object());
}
