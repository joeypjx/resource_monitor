#include <iostream>
#include <string>
#include <cstdlib>
#include "agent/agent.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    // 初始化日志
    Logger::initialize("agent", "agent.log");

    // 默认参数
    std::string manager_url = "http://localhost:8080";
    std::string hostname = "";
    int collection_interval_sec = 5;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--manager-url" && i + 1 < argc) {
            manager_url = argv[++i];
        } else if (arg == "--hostname" && i + 1 < argc) {
            hostname = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            collection_interval_sec = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            LOG_INFO("Usage: agent [options]");
            LOG_INFO("Options:");
            LOG_INFO("  --manager-url <url>    Manager URL (default: http://localhost:8080)");
            LOG_INFO("  --hostname <name>      Override hostname");
            LOG_INFO("  --interval <seconds>   Collection interval in seconds (default: 5)");
            LOG_INFO("  --help                 Show this help message");
            return 0;
        }
    }
    
    // 创建Agent实例
    Agent agent(manager_url, hostname, collection_interval_sec);
    
    // 启动Agent
    if (!agent.start()) {
        LOG_ERROR("Failed to start agent");
        return 1;
    }
    
    LOG_INFO("Press Ctrl+C to stop...");
    
    // 等待信号
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
