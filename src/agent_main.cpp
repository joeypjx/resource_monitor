#include <iostream>
#include <string>
#include <cstdlib>
#include "agent/agent.h"
#include "utils/logger.h"

/**
 * @brief Agent主程序入口
 * 
 * @param argc 参数个数
 * @param argv 参数列表
 * @return int 退出码
 * 
 * Agent负责在受控节点上收集资源信息，并上报给Manager。
 * 支持通过命令行参数进行配置。
 */
int main(int argc, char* argv[]) {
    // 初始化日志系统
    Logger::initialize("agent", "agent.log");

    // 默认配置参数
    std::string manager_url = "http://localhost:8080"; // Manager服务地址
    std::string hostname = ""; // 自定义主机名，如果为空则自动获取
    std::string network_interface = ""; // 网络接口名称，如果为空则自动检测
    int collection_interval_sec = 5; // 数据收集间隔，单位秒
    int port = 8081; // Agent本地HTTP服务端口
    
    // 解析命令行参数，覆盖默认配置
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--manager-url" && i + 1 < argc) {
            manager_url = argv[++i];
        } else if (arg == "--hostname" && i + 1 < argc) {
            hostname = argv[++i];
        } else if (arg == "--network-interface" && i + 1 < argc) {
            network_interface = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            collection_interval_sec = std::atoi(argv[++i]);
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            LOG_INFO("Usage: agent [options]");
            LOG_INFO("Options:");
            LOG_INFO("  --manager-url <url>         Manager URL (default: http://localhost:8080)");
            LOG_INFO("  --hostname <name>           Override hostname");
            LOG_INFO("  --network-interface <name>  Network interface name (default: auto-detect)");
            LOG_INFO("  --interval <seconds>        Collection interval in seconds (default: 5)");
            LOG_INFO("  --port <port>               Agent local port (default: 8081)");
            LOG_INFO("  --help                      Show this help message");
            return 0;
        }
    }
    
    // 创建Agent实例
    // Agent对象封装了所有Agent端的核心逻辑
    Agent agent(manager_url, hostname, collection_interval_sec, port, network_interface);
    
    // 启动Agent服务
    if (!agent.start()) {
        LOG_ERROR("Failed to start agent");
        return 1;
    }
    
    LOG_INFO("Press Ctrl+C to stop...");
    
    // 保持主线程运行，等待中断信号
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
