#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <map>
#include <nlohmann/json.hpp>


// @class HttpClient
// @brief HTTP客户端

// 封装了Agent与Manager之间的所有HTTP通信。
// 这个类提供了一系列简单的方法，用于调用Manager定义的特定RESTful API端点。
// - 注册Agent (`registerAgent`)
// - 上报资源数据 (`reportData`)
// - 发送心跳 (`heartbeat`)

// 底层使用 `cpp-httplib` 来执行实际的HTTP请求。

class HttpClient {
public:

// @brief 默认构造函数

    HttpClient() = default;
    

// @brief 构造函数
// @param base_url Manager服务的基础URL，例如 "http://manager-ip:8080"

    explicit HttpClient(const std::string& base_url);
    

// @brief 析构函数

    ~HttpClient() = default;
    

// @brief 向Manager注册Agent
// 发送一个POST请求到 `/api/node/register`。
// @param agent_info 包含Agent静态信息的JSON对象 (e.g., hostname, os_info)。
// @return nlohmann::json Manager的响应。成功时应包含一个 `agent_id`。

    nlohmann::json registerAgent(const nlohmann::json& agent_info);
    

// @brief 上报采集到的资源数据
// 发送一个POST请求到 `/api/node/resources`。
// @param resource_data 包含周期性采集的资源数据 (e.g., cpu_usage, mem_usage) 的JSON对象。
// @return nlohmann::json Manager的响应。

    nlohmann::json reportData(const nlohmann::json& resource_data);


// @brief 发送心跳请求
// 发送一个POST请求到 `/api/node/heartbeat`，以告知Manager该Agent仍然在线。
// @param node_id 由Manager分配的Agent唯一ID。
// @return nlohmann::json Manager的响应。

    nlohmann::json heartbeat(const std::string& node_id);
    

// @brief 发送一个通用的HTTP GET请求
// @param endpoint API端点 (e.g., "/api/some/data")。
// @param headers 自定义的HTTP请求头。
// @return nlohmann::json 服务器响应体解析后的JSON对象。

    nlohmann::json get(const std::string& endpoint, 
                       const std::map<std::string, std::string>& headers = {});
    

// @brief 发送一个通用的HTTP POST请求
// @param endpoint API端点 (e.g., "/api/some/action")。
// @param data 要在请求体中发送的JSON数据。
// @param headers 自定义的HTTP请求头。
// @return nlohmann::json 服务器响应体解析后的JSON对象。

    nlohmann::json post(const std::string& endpoint, 
                        const nlohmann::json& data,
                        const std::map<std::string, std::string>& headers = {});

private:
    std::string base_url_;  // Manager服务的基础URL
};

#endif // HTTP_CLIENT_H
