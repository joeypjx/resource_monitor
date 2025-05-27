#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <map>
#include <nlohmann/json.hpp>

/**
 * HttpClient类 - HTTP客户端
 * 
 * 负责Agent与Manager之间的HTTP通信
 */
class HttpClient {
public:
    /**
     * 构造函数
     * 
     * @param base_url 基础URL，如"http://manager:8080"
     */
    explicit HttpClient(const std::string& base_url);
    
    /**
     * 析构函数
     */
    ~HttpClient() = default;
    
    /**
     * 发送注册请求
     * 
     * @param agent_info 包含Agent信息的JSON对象
     * @return 服务器响应的JSON对象
     */
    nlohmann::json registerAgent(const nlohmann::json& agent_info);
    
    /**
     * 上报资源数据
     * 
     * @param resource_data 包含资源数据的JSON对象
     * @return 服务器响应的JSON对象
     */
    nlohmann::json reportData(const nlohmann::json& resource_data);
    
    /**
     * 发送HTTP GET请求
     * 
     * @param endpoint API端点
     * @param headers 请求头
     * @return 响应内容的JSON对象
     */
    nlohmann::json get(const std::string& endpoint, 
                       const std::map<std::string, std::string>& headers = {});
    
    /**
     * 发送HTTP POST请求
     * 
     * @param endpoint API端点
     * @param data 请求体数据
     * @param headers 请求头
     * @return 响应内容的JSON对象
     */
    nlohmann::json post(const std::string& endpoint, 
                        const nlohmann::json& data,
                        const std::map<std::string, std::string>& headers = {});

    /**
     * 发送心跳请求
     * 
     * @param board_id Board的ID
     * @return 服务器响应的JSON对象
     */
    nlohmann::json heartbeat(const std::string& board_id);

private:
    std::string base_url_;  // 基础URL
};

#endif // HTTP_CLIENT_H
