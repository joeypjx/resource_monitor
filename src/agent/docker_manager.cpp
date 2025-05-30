#include "docker_manager.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <curl/curl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// 辅助函数：执行系统命令并获取输出
static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// 辅助函数：CURL写回调
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch(std::bad_alloc &e) {
        return 0;
    }
    return newLength;
}

DockerManager::DockerManager() 
    : docker_socket_path_("/var/run/docker.sock"), use_api_(true) {
}

bool DockerManager::initialize() {
    // 检查Docker守护进程是否可用
    if (!checkDockerAvailable()) {
        std::cerr << "Docker daemon is not available" << std::endl;
        return false;
    }
    
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_ALL);
    
    return true;
}

bool DockerManager::checkDockerAvailable() {
    try {
        // 尝试执行docker info命令
        std::string result = exec("docker info 2>/dev/null");
        if (result.find("Server Version") != std::string::npos) {
            return true;
        }
        
        // 如果命令行方式失败，尝试通过API检查
        nlohmann::json response = dockerApiRequest("GET", "/info");
        return !response.empty() && response.contains("ServerVersion");
    } catch (const std::exception& e) {
        std::cerr << "Error checking Docker availability: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json DockerManager::pullImage(const std::string& image_url, const std::string& image_name) {
    try {
        std::string cmd;
        
        // 如果提供了URL，则从URL加载镜像
        if (!image_url.empty()) {
            // 下载镜像文件
            std::string download_cmd = "curl -s -L -o /tmp/image.tar " + image_url;
            int download_result = system(download_cmd.c_str());
            if (download_result != 0) {
                return {
                    {"status", "error"},
                    {"message", "Failed to download image from URL"}
                };
            }
            
            // 加载镜像
            cmd = "docker load -i /tmp/image.tar";
            std::string load_output = exec(cmd.c_str());
            
            // 清理临时文件
            system("rm -f /tmp/image.tar");
            
            return {
                {"status", "success"},
                {"message", "Image loaded successfully"},
                {"output", load_output}
            };
        } 
        // 否则，从Docker Hub拉取镜像
        else if (!image_name.empty()) {

            // 检查本地有没有镜像
            std::string check_cmd = "docker images " + image_name;
            std::string check_output = exec(check_cmd.c_str());
            if (check_output.find(image_name) != std::string::npos) {
                return {
                    {"status", "success"},
                    {"message", "Image already exists"},
                    {"output", check_output}
                };
            }

            // 拉取镜像
            cmd = "docker pull " + image_name;
            std::string pull_output = exec(cmd.c_str());
            
            if (pull_output.find("Error") != std::string::npos) {
                return {
                    {"status", "error"},
                    {"message", "Failed to pull image"},
                    {"output", pull_output}
                };
            }
            
            return {
                {"status", "success"},
                {"message", "Image pulled successfully"},
                {"output", pull_output}
            };
        } else {
            return {
                {"status", "error"},
                {"message", "No image URL or name provided"}
            };
        }
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error pulling image: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::createContainer(const std::string& image_name,
                                           const std::string& container_name,
                                           const nlohmann::json& env_vars,
                                           const nlohmann::json& resource_limits,
                                           const std::vector<std::string>& volumes) {
    try {
        // 构建创建容器的命令
        std::stringstream cmd;
        cmd << "docker run -d";
        
        // 添加容器名称
        if (!container_name.empty()) {
            cmd << " --name " << container_name;
        }
        
        // 添加环境变量
        if (env_vars.is_object()) {
            for (auto it = env_vars.begin(); it != env_vars.end(); ++it) {
                cmd << " -e " << it.key() << "=" << it.value().get<std::string>();
            }
        }
        
        // 添加资源限制
        if (resource_limits.is_object()) {
            // CPU限制
            if (resource_limits.contains("cpu_cores")) {
                double cpu_cores = resource_limits["cpu_cores"].get<double>();
                cmd << " --cpus=" << cpu_cores;
            }
            
            // 内存限制
            if (resource_limits.contains("memory_mb")) {
                int memory_mb = resource_limits["memory_mb"].get<int>();
                cmd << " --memory=" << memory_mb << "m";
            }
        }
        
        // 添加卷挂载
        for (const auto& volume : volumes) {
            cmd << " -v " << volume;
        }
        
        // 添加镜像名称
        cmd << " " << image_name;
        
        // 执行命令
        std::string output = exec(cmd.str().c_str());
        
        // 检查输出是否包含容器ID（40个字符的哈希值）
        if (output.length() >= 12) {
            std::string container_id = output.substr(0, 12);
            
            return {
                {"status", "success"},
                {"message", "Container created successfully"},
                {"container_id", container_id}
            };
        } else {
            return {
                {"status", "error"},
                {"message", "Failed to create container"},
                {"output", output}
            };
        }
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error creating container: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::stopContainer(const std::string& container_id) {
    try {
        // 构建停止容器的命令
        std::string cmd = "docker stop " + container_id;
        std::string output = exec(cmd.c_str());
        
        // 检查输出是否包含容器ID
        if (output.find(container_id) != std::string::npos) {
            return {
                {"status", "success"},
                {"message", "Container stopped successfully"}
            };
        } else {
            return {
                {"status", "error"},
                {"message", "Failed to stop container"},
                {"output", output}
            };
        }
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error stopping container: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::removeContainer(const std::string& container_id) {
    try {
        // 构建删除容器的命令
        std::string cmd = "docker rm -f " + container_id;
        std::string output = exec(cmd.c_str());
        
        // 检查输出是否包含容器ID
        if (output.find(container_id) != std::string::npos) {
            return {
                {"status", "success"},
                {"message", "Container removed successfully"}
            };
        } else {
            return {
                {"status", "error"},
                {"message", "Failed to remove container"},
                {"output", output}
            };
        }
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error removing container: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::getContainerStatus(const std::string& container_id) {
    try {
        // 构建获取容器状态的命令
        std::string cmd = "docker inspect --format='{{.State.Status}}' " + container_id;
        std::string status = exec(cmd.c_str());
        
        // 去除末尾的换行符
        if (!status.empty() && status[status.length() - 1] == '\n') {
            status.erase(status.length() - 1);
        }
        
        // 检查是否获取到状态
        if (status.empty() || status.find("Error") != std::string::npos) {
            return {
                {"status", "error"},
                {"message", "Failed to get container status"},
                {"output", status}
            };
        } else {
            return {
                {"status", "success"},
                {"container_status", status}
            };
        }
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error getting container status: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::getContainerStats(const std::string& container_id) {
    try {
        nlohmann::json stats;
        
        // 获取CPU使用率
        std::string cpu_cmd = "docker stats --no-stream --format '{{.CPUPerc}}' " + container_id;
        std::string cpu_output = exec(cpu_cmd.c_str());
        
        // 去除末尾的换行符和百分号
        if (!cpu_output.empty()) {
            if (cpu_output[cpu_output.length() - 1] == '\n') {
                cpu_output.erase(cpu_output.length() - 1);
            }
            if (cpu_output[cpu_output.length() - 1] == '%') {
                cpu_output.erase(cpu_output.length() - 1);
            }
            
            try {
                stats["cpu_percent"] = std::stod(cpu_output);
            } catch (...) {
                stats["cpu_percent"] = 0.0;
            }
        } else {
            stats["cpu_percent"] = 0.0;
        }
        
        // 获取内存使用量
        std::string mem_cmd = "docker stats --no-stream --format '{{.MemUsage}}' " + container_id;
        std::string mem_output = exec(mem_cmd.c_str());
        
        // 解析内存使用量，格式如 "100MiB / 2GiB"
        if (!mem_output.empty()) {
            size_t pos = mem_output.find(" / ");
            if (pos != std::string::npos) {
                std::string mem_used = mem_output.substr(0, pos);
                
                // 转换为MB
                double memory_mb = 0.0;
                if (mem_used.find("GiB") != std::string::npos) {
                    memory_mb = std::stod(mem_used.substr(0, mem_used.find("GiB"))) * 1024;
                } else if (mem_used.find("MiB") != std::string::npos) {
                    memory_mb = std::stod(mem_used.substr(0, mem_used.find("MiB")));
                } else if (mem_used.find("KiB") != std::string::npos) {
                    memory_mb = std::stod(mem_used.substr(0, mem_used.find("KiB"))) / 1024;
                }
                
                stats["memory_mb"] = memory_mb;
            } else {
                stats["memory_mb"] = 0;
            }
        } else {
            stats["memory_mb"] = 0;
        }
        
        // GPU使用率（简化处理，实际应该使用nvidia-smi等工具）
        stats["gpu_percent"] = 0.0;
        
        return {
            {"status", "success"},
            {"resource_usage", stats}
        };
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error getting container stats: ") + e.what()}
        };
    }
}

nlohmann::json DockerManager::listContainers(bool all) {
    try {
        // 构建列出容器的命令
        std::string cmd = "docker ps --format '{{.ID}}|{{.Names}}|{{.Status}}|{{.Image}}'";
        if (all) {
            cmd += " -a";
        }
        
        std::string output = exec(cmd.c_str());
        std::istringstream iss(output);
        std::string line;
        
        nlohmann::json containers = nlohmann::json::array();
        
        while (std::getline(iss, line)) {
            size_t pos1 = line.find("|");
            size_t pos2 = line.find("|", pos1 + 1);
            size_t pos3 = line.find("|", pos2 + 1);
            
            if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos) {
                std::string id = line.substr(0, pos1);
                std::string name = line.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string status = line.substr(pos2 + 1, pos3 - pos2 - 1);
                std::string image = line.substr(pos3 + 1);
                
                nlohmann::json container;
                container["id"] = id;
                container["name"] = name;
                container["status"] = status;
                container["image"] = image;
                
                containers.push_back(container);
            }
        }
        
        return {
            {"status", "success"},
            {"containers", containers}
        };
    } catch (const std::exception& e) {
        return {
            {"status", "error"},
            {"message", std::string("Error listing containers: ") + e.what()}
        };
    }
}

std::string DockerManager::executeDockerCommand(const std::string& command) {
    return exec(command.c_str());
}

nlohmann::json DockerManager::dockerApiRequest(const std::string& method, 
                                            const std::string& endpoint, 
                                            const nlohmann::json& request_body) {
    CURL* curl = curl_easy_init();
    std::string response_string;
    
    if (curl) {
        std::string url = "http://localhost/v1.40" + endpoint;
        
        // 设置UNIX套接字
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, docker_socket_path_.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // 设置HTTP方法
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        } else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        // 设置请求体
        std::string request_body_str;
        if (!request_body.empty()) {
            request_body_str = request_body.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body_str.c_str());
            
            // 设置Content-Type
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        
        // 设置回调函数
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        
        // 执行请求
        CURLcode res = curl_easy_perform(curl);
        
        // 检查结果
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return nlohmann::json();
        }
        
        // 清理
        curl_easy_cleanup(curl);
        
        // 解析响应
        try {
            return nlohmann::json::parse(response_string);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            return nlohmann::json();
        }
    }
    
    return nlohmann::json();
}
