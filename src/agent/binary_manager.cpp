#include "binary_manager.h"
#include "utils/logger.h"
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
#include <sys/wait.h>
#include <cstring>
#include <vector>
#include <cerrno>
#include "sftp_client.h"

// 用于curl下载的回调函数
size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

BinaryManager::BinaryManager() 
    : process_map_()
    , process_mutex_()
    , sftp_client_(nullptr)
{
    // 初始化curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    sftp_client_ = std::make_unique<SFTPClient>();
}

BinaryManager::~BinaryManager() {
    // 清理curl
    curl_global_cleanup();
}

bool BinaryManager::initialize() {
    LOG_INFO("Initializing BinaryManager...");
    return true;
}

nlohmann::json BinaryManager::downloadBinary(const std::string& binary_url, const std::string& binary_path) {
    LOG_INFO("Downloading binary from {} to {}", binary_url, binary_path);
    
    // 创建目录（如果不存在）
    size_t last_slash = binary_path.find_last_of('/');
    std::string parent_dir = (last_slash != std::string::npos) ? binary_path.substr(0, last_slash) : ".";
    create_directories(parent_dir);
    
    // 判断协议
    if (binary_url.rfind("sftp://", 0) == 0) {
        std::string err_msg = "";
        bool ret_ok = sftp_client_->downloadFile(binary_url, binary_path, err_msg);
        if (!ret_ok) {
            return {
                {"status", "error"},
                {"message", "SFTP下载失败: " + err_msg}
            };
        }
    } else {
        // 原有curl下载逻辑
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {
                {"status", "error"},
                {"message", "Failed to initialize curl"}
            };
        }
        FILE* file_ptr = fopen(binary_path.c_str(), "wb");
        if (!file_ptr) {
            curl_easy_cleanup(curl);
            return {
                {"status", "error"},
                {"message", "Failed to create file: " + binary_path}
            };
        }
        curl_easy_setopt(curl, CURLOPT_URL, binary_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file_ptr);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5分钟超时
        CURLcode res = curl_easy_perform(curl);
        fclose(file_ptr);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            return {
                {"status", "error"},
                {"message", "Failed to download file: " + std::string(curl_easy_strerror(res))}
            };
        }
        curl_easy_cleanup(curl);
    }
    
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

nlohmann::json BinaryManager::startProcess(const std::string& binary_path, const std::string& working_dir, const std::vector<std::string>& command_args, const nlohmann::json& env_vars) {
    pid_t pid = fork();
    if (pid < 0) {
        // fork失败
        return {
            {"status", "error"},
            {"message", std::string("fork failed: ") + strerror(errno)}
        };
    }
    if (pid == 0) {
        // 子进程
        // 设置工作目录
        if (!working_dir.empty()) {
            if (chdir(working_dir.c_str()) != 0) {
                _exit(127);
            }
        }
        // 构造参数
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary_path.c_str()));
        for (const auto& arg : command_args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        // 构造环境变量
        std::vector<std::string> env_strs;
        std::vector<char*> envp;
        for (auto it = env_vars.begin(); it != env_vars.end(); ++it) {
            env_strs.push_back(it.key() + "=" + it.value().get<std::string>());
        }
        for (auto& s : env_strs) {
            envp.push_back(const_cast<char*>(s.c_str()));
        }
        envp.push_back(nullptr);
        // 重定向输出到 /dev/null
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        // 执行
        execve(binary_path.c_str(), argv.data(), envp.data());
        // execve失败
        _exit(127);
    }
    // 父进程
    // 保存pid为string
    std::string pid_str = std::to_string(pid);
    {
        std::lock_guard<std::mutex> lock(process_mutex_);
        process_map_[pid_str] = binary_path;
    }
    return {
        {"status", "success"},
        {"process_id", pid_str}
    };
}

nlohmann::json BinaryManager::stopProcess(const std::string& process_id) {
    int pid = std::stoi(process_id);
    LOG_INFO("Stopping process: {}", pid);
    if (!isProcessRunning(process_id)) {
        return {
            {"status", "error"},
            {"message", "Process not found: " + process_id}
        };
    }
    kill(pid, SIGTERM);
    int waited = 0;
    int max_wait = 5;
    while (waited < max_wait) {
        int ret = waitpid(pid, nullptr, WNOHANG);
        if (ret == pid) break;
        sleep(1);
        waited++;
    }
    if (waited == max_wait) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }
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

nlohmann::json BinaryManager::getProcessStatus(const std::string& process_id) {
    bool running = isProcessRunning(process_id);
    std::string binary_path = "";
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

nlohmann::json BinaryManager::getProcessStats(const std::string& process_id) {
    if (!isProcessRunning(process_id)) {
        return {
            {"status", "error"},
            {"message", "Process not found: " + process_id}
        };
    }
    std::string cmd = "ps -p " + process_id + " -o %cpu,%mem,rss --no-headers";
    std::string output = executeCommand(cmd);
    std::istringstream iss(output);
    float cpu_percent = 0.0f, mem_percent = 0.0f;
    long rss_kb = 0;
    iss >> cpu_percent >> mem_percent >> rss_kb;
    return {
        {"process_id", process_id},
        {"cpu_percent", cpu_percent},
        {"memory_percent", mem_percent},
        {"memory_rss_kb", rss_kb}
    };
}

std::string BinaryManager::executeCommand(const std::string& command) {
    std::string result = "";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[128] = {0};
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    
    // 确保pipe不为NULL后再关闭
    if (pipe) {
        pclose(pipe);
    }

    // 去掉换行符
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

bool BinaryManager::extractFile(const std::string& file_path, const std::string& extract_dir) {
    std::string cmd = "tar -xzf " + file_path + " -C " + extract_dir;
    int result = system(cmd.c_str());
    return result == 0;
}

bool BinaryManager::isProcessRunning(const std::string& process_id) {
    std::string cmd = "ps -o stat= -p " + process_id + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[128] = {0};
    std::string status = "";
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        status = buffer;
    }
    
    // 确保pipe不为NULL后再关闭
    if (pipe) {
        pclose(pipe);
    }
    
    status.erase(0, status.find_first_not_of(" \t\n\r"));
    status.erase(status.find_last_not_of(" \t\n\r") + 1);
    if (status.empty() || status.find('Z') != std::string::npos) {
        return false;
    }
    return true;
}
