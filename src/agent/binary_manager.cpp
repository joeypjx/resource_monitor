#include "binary_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <curl/curl.h>
#include "dir_utils.h"

// 用于curl下载的回调函数
size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

BinaryManager::BinaryManager() {
    // 初始化curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

BinaryManager::~BinaryManager() {
    // 清理curl
    curl_global_cleanup();
}

bool BinaryManager::initialize() {
    std::cout << "Initializing BinaryManager..." << std::endl;
    return true;
}

nlohmann::json BinaryManager::downloadBinary(const std::string& binary_url, const std::string& binary_path) {
    std::cout << "Downloading binary from " << binary_url << " to " << binary_path << std::endl;
    
    // 创建目录（如果不存在）
    size_t last_slash = binary_path.find_last_of('/');
    std::string parent_dir = (last_slash != std::string::npos) ? binary_path.substr(0, last_slash) : ".";
    create_directories(parent_dir);
    
    // 下载文件
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {
            {"status", "error"},
            {"message", "Failed to initialize curl"}
        };
    }
    
    FILE* fp = fopen(binary_path.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return {
            {"status", "error"},
            {"message", "Failed to create file: " + binary_path}
        };
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, binary_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5分钟超时
    
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return {
            {"status", "error"},
            {"message", "Failed to download file: " + std::string(curl_easy_strerror(res))}
        };
    }
    
    curl_easy_cleanup(curl);
    
    // 检查文件是否为压缩包，如果是则解压
    bool is_tar_gz = (binary_path.size() >= 7 && binary_path.substr(binary_path.size() - 7) == ".tar.gz");
    bool is_tgz = (binary_path.size() >= 4 && binary_path.substr(binary_path.size() - 4) == ".tgz");
    if (is_tar_gz || is_tgz) {
        std::string extract_dir = parent_dir;
        if (!extractFile(binary_path, extract_dir)) {
            return {
                {"status", "error"},
                {"message", "Failed to extract file: " + binary_path}
            };
        }
    }
    
    // 设置执行权限
    chmod(binary_path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    
    return {
        {"status", "success"},
        {"message", "Binary downloaded successfully"},
        {"binary_path", binary_path}
    };
}

nlohmann::json BinaryManager::startProcess(const std::string& binary_path, 
                                         const std::string& working_dir,
                                         const std::vector<std::string>& command_args,
                                         const nlohmann::json& env_vars) {
    std::cout << "Starting process: " << binary_path << std::endl;
    
    // 检查二进制文件是否存在
    if (access(binary_path.c_str(), F_OK) != 0) {
        return {
            {"status", "error"},
            {"message", "Binary file not found: " + binary_path}
        };
    }
    
    // 创建工作目录（如果不存在）
    create_directories(working_dir);
    
    // 构建命令行
    std::string cmd = binary_path;
    for (const auto& arg : command_args) {
        cmd += " " + arg;
    }
    
    // 添加环境变量
    std::string env_cmd;
    for (auto it = env_vars.begin(); it != env_vars.end(); ++it) {
        env_cmd += it.key() + "=\"" + it.value().get<std::string>() + "\" ";
    }
    
    // 构建完整命令（后台运行）
    std::string full_cmd = "cd " + working_dir + " && " + env_cmd + cmd + " > /dev/null 2>&1 & echo $!";
    
    // 执行命令并获取进程ID
    std::string output = executeCommand(full_cmd);
    int process_id = std::stoi(output);
    
    // 记录进程信息
    {
        std::lock_guard<std::mutex> lock(process_mutex_);
        process_map_[process_id] = binary_path;
    }

    return {
        {"status", "success"},
        {"message", "Process started successfully"},
        {"process_id", process_id},
        {"binary_path", binary_path}
    };
}

nlohmann::json BinaryManager::stopProcess(int process_id) {
    std::cout << "Stopping process: " << process_id << std::endl;
    
    // 检查进程是否存在
    if (!isProcessRunning(process_id)) {
        return {
            {"status", "error"},
            {"message", "Process not found: " + std::to_string(process_id)}
        };
    }
    
    // 发送SIGTERM信号
    if (kill(process_id, SIGTERM) != 0) {
        return {
            {"status", "error"},
            {"message", "Failed to send SIGTERM to process: " + std::to_string(process_id)}
        };
    }
    
    // 等待进程结束（最多5秒）
    for (int i = 0; i < 5; i++) {
        if (!isProcessRunning(process_id)) {
            break;
        }
        sleep(1);
    }
    
    // 如果进程仍在运行，发送SIGKILL信号
    if (isProcessRunning(process_id)) {
        if (kill(process_id, SIGKILL) != 0) {
            return {
                {"status", "error"},
                {"message", "Failed to send SIGKILL to process: " + std::to_string(process_id)}
            };
        }
    }
    
    // 从进程映射中移除
    {
        std::lock_guard<std::mutex> lock(process_mutex_);
        process_map_.erase(process_id);
    }
    
    return {
        {"status", "success"},
        {"message", "Process stopped successfully"},
        {"process_id", process_id}
    };
}

nlohmann::json BinaryManager::getProcessStatus(int process_id) {
    // 检查进程是否存在
    bool running = isProcessRunning(process_id);
    
    std::string binary_path;
    {
        std::lock_guard<std::mutex> lock(process_mutex_);
        auto it = process_map_.find(process_id);
        if (it != process_map_.end()) {
            binary_path = it->second;
        }
    }
    
    return {
        {"process_id", process_id},
        {"running", running},
        {"binary_path", binary_path}
    };
}

nlohmann::json BinaryManager::getProcessStats(int process_id) {
    // 检查进程是否存在
    if (!isProcessRunning(process_id)) {
        return {
            {"status", "error"},
            {"message", "Process not found: " + std::to_string(process_id)}
        };
    }
    
    // 获取CPU和内存使用情况
    std::string cmd = "ps -p " + std::to_string(process_id) + " -o %cpu,%mem,rss --no-headers";
    std::string output = executeCommand(cmd);
    
    // 解析输出
    std::istringstream iss(output);
    float cpu_percent, mem_percent;
    long rss_kb;
    
    iss >> cpu_percent >> mem_percent >> rss_kb;
    
    return {
        {"process_id", process_id},
        {"cpu_percent", cpu_percent},
        {"memory_percent", mem_percent},
        {"memory_rss_kb", rss_kb}
    };
}

std::string BinaryManager::executeCommand(const std::string& command) {
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);

    // 去掉换行符
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

bool BinaryManager::extractFile(const std::string& file_path, const std::string& extract_dir) {
    std::string cmd = "tar -xzf " + file_path + " -C " + extract_dir;
    int result = system(cmd.c_str());
    return result == 0;
}

bool BinaryManager::isProcessRunning(int process_id) {
    std::string cmd = "ps -p " + std::to_string(process_id) + " > /dev/null";
    int result = system(cmd.c_str());
    return result == 0;
}
