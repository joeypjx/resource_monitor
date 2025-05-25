#include "agent_control_manager.h"
#include "database_manager.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

AgentControlManager::AgentControlManager(std::shared_ptr<DatabaseManager> db_manager)
    : db_manager_(db_manager) {}

nlohmann::json AgentControlManager::controlAgent(const std::string& agent_id, const nlohmann::json& request_json) {
    // 1. 查找agent的ip地址
    std::cout << "controlAgent: " << agent_id << std::endl;
    
    auto agent_info = db_manager_->getNode(agent_id);
    if (agent_info.is_null() || !agent_info.contains("ip_address")) {
        return {{"status", "error"}, {"message", "Agent not found or missing ip_address"}};
    }
    std::string ip = agent_info["ip_address"];
    int port = 8081; // 假设agent http端口为8081
    std::string url = "/api/node/control";

    // 2. 调用agent的节点控制接口
    httplib::Client cli(ip, port);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(5);
    auto res = cli.Post(url.c_str(), request_json.dump(), "application/json");
    if (!res) {
        return {{"status", "error"}, {"message", "Failed to connect to agent"}};
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (...) {
        return {{"status", "error"}, {"message", "Invalid response from agent"}};
    }
} 