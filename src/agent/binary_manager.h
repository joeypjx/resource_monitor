#ifndef BINARY_MANAGER_H
#define BINARY_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include "sftp_client.h"


// BinaryManager类 - 二进制运行体管理器
// 
// 负责管理二进制运行体的生命周期，包括下载、启动、停止和状态收集

class BinaryManager {
public:

// 构造函数

    BinaryManager();
    

// 析构函数

    ~BinaryManager();
    

// 初始化二进制运行体管理器
// 
// @return 是否成功初始化

    bool initialize();
    

// 下载二进制文件
// 
// @param binary_url 二进制文件URL
// @param binary_path 保存路径
// @return 下载结果

    nlohmann::json downloadBinary(const std::string& binary_url, const std::string& binary_path);
    

// 启动进程
// 
// @param binary_path 二进制文件路径
// @param working_dir 工作目录
// @param command_args 命令行参数
// @param env_vars 环境变量
// @return 启动结果，包含进程ID

    nlohmann::json startProcess(const std::string& binary_path, 
                              const std::string& working_dir,
                              const std::vector<std::string>& command_args,
                              const nlohmann::json& env_vars);
    

// 停止进程
// 
// @param process_id 进程ID
// @return 停止结果

    nlohmann::json stopProcess(const std::string& process_id);
    

// 获取进程状态
// 
// @param process_id 进程ID
// @return 进程状态

    nlohmann::json getProcessStatus(const std::string& process_id);
    

// 获取进程资源使用统计
// 
// @param process_id 进程ID
// @return 资源使用统计

    nlohmann::json getProcessStats(const std::string& process_id);

private:

// 执行命令并返回输出
// 
// @param command 命令
// @return 命令输出

    std::string executeCommand(const std::string& command);
    

// 解压文件
// 
// @param file_path 文件路径
// @param extract_dir 解压目录
// @return 是否成功解压

    bool extractFile(const std::string& file_path, const std::string& extract_dir);
    

// 检查进程是否存在
// 
// @param process_id 进程ID
// @return 是否存在

    bool isProcessRunning(const std::string& process_id);

private:
    std::map<std::string, std::string> process_map_;  // 进程ID到二进制路径的映射，key为string
    std::mutex process_mutex_;
    std::unique_ptr<SFTPClient> sftp_client_; // SFTP客户端
};

#endif // BINARY_MANAGER_H
