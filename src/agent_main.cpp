#include <iostream>
#include <string>
#include <cstdlib>
#include "agent/agent.h"

int main(int argc, char* argv[]) {
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
            std::cout << "Usage: agent [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --manager-url <url>    Manager URL (default: http://localhost:8080)" << std::endl;
            std::cout << "  --hostname <name>      Override hostname" << std::endl;
            std::cout << "  --interval <seconds>   Collection interval in seconds (default: 5)" << std::endl;
            std::cout << "  --help                 Show this help message" << std::endl;
            return 0;
        }
    }
    
    // 创建Agent实例
    Agent agent(manager_url, hostname, collection_interval_sec);
    
    // 启动Agent
    if (!agent.start()) {
        std::cerr << "Failed to start agent" << std::endl;
        return 1;
    }
    
    std::cout << "Agent started with ID: " << agent.getAgentId() << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // 等待信号
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
