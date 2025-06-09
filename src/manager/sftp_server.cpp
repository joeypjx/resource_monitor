#include "sftp_server.h"
#include <iostream>
#include <cstring>
#include <sys/wait.h>

SFTPServer::SFTPServer(int port, const std::string& root_dir)
    : port_(port), root_dir_(root_dir), running_(false) {}

SFTPServer::~SFTPServer() {
    stop();
}

bool SFTPServer::start() {
    if (running_) return false;
    running_ = true;
    server_thread_ = std::thread(&SFTPServer::serverThreadFunc, this);
    return true;
}

void SFTPServer::stop() {
    if (!running_) return;
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    // 清理libssh资源
    if (sftp_) {
        sftp_free(sftp_);
        sftp_ = nullptr;
    }
    if (session_) {
        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = nullptr;
    }
    if (sshbind_) {
        ssh_bind_free(sshbind_);
        sshbind_ = nullptr;
    }
}

bool SFTPServer::isRunning() const {
    return running_;
}

void SFTPServer::serverThreadFunc() {
    std::cout << "[SFTPServer] SFTP服务线程启动，监听端口: " << port_ << std::endl;
    // 初始化SSH bind
    sshbind_ = ssh_bind_new();
    if (!sshbind_) {
        std::cerr << "[SFTPServer] 创建SSH bind失败" << std::endl;
        return;
    }
    // 设置监听端口
    ssh_bind_options_set(sshbind_, SSH_BIND_OPTIONS_BINDADDR, "0.0.0.0");
    ssh_bind_options_set(sshbind_, SSH_BIND_OPTIONS_PORT, &port_);
    // 设置host key（需提前生成密钥文件）
    std::string key_path = root_dir_ + "/ssh_host_rsa_key";
    ssh_bind_options_set(sshbind_, SSH_BIND_OPTIONS_RSAKEY, key_path.c_str());
    // 启动监听
    if (ssh_bind_listen(sshbind_) < 0) {
        std::cerr << "[SFTPServer] 监听端口失败: " << ssh_get_error(sshbind_) << std::endl;
        return;
    }
    while (running_) {
        session_ = ssh_new();
        if (!session_) {
            std::cerr << "[SFTPServer] 创建SSH session失败" << std::endl;
            continue;
        }
        if (ssh_bind_accept(sshbind_, session_) != SSH_OK) {
            std::cerr << "[SFTPServer] 接受连接失败: " << ssh_get_error(sshbind_) << std::endl;
            ssh_free(session_);
            session_ = nullptr;
            continue;
        }
        if (ssh_handle_key_exchange(session_) != SSH_OK) {
            std::cerr << "[SFTPServer] 密钥交换失败: " << ssh_get_error(session_) << std::endl;
            ssh_disconnect(session_);
            ssh_free(session_);
            session_ = nullptr;
            continue;
        }
        // 简单密码认证（用户名/密码都为test，实际可扩展）
        bool authenticated = false;
        while (!authenticated) {
            ssh_message message = ssh_message_get(session_);
            if (!message) break;
            if (ssh_message_type(message) == SSH_REQUEST_AUTH &&
                ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD) {
                const char* user = ssh_message_auth_user(message);
                const char* pass = ssh_message_auth_password(message);
                if (strcmp(user, "test") == 0 && strcmp(pass, "test") == 0) {
                    ssh_message_auth_reply_success(message, 0);
                    authenticated = true;
                } else {
                    ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_PASSWORD);
                    ssh_message_reply_default(message);
                }
            } else {
                ssh_message_reply_default(message);
            }
            ssh_message_free(message);
        }
        if (!authenticated) {
            ssh_disconnect(session_);
            ssh_free(session_);
            session_ = nullptr;
            continue;
        }
        // 等待SFTP子系统请求
        ssh_channel chan = nullptr;
        bool sftp_started = false;
        while (!sftp_started) {
            ssh_message message = ssh_message_get(session_);
            if (!message) break;
            if (ssh_message_type(message) == SSH_REQUEST_CHANNEL &&
                ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
                chan = ssh_message_channel_request_open_reply_accept(message);
            } else if (ssh_message_type(message) == SSH_REQUEST_CHANNEL &&
                       ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SUBSYSTEM &&
                       strcmp(ssh_message_channel_request_subsystem(message), "sftp") == 0) {
                ssh_message_channel_request_reply_success(message);
                sftp_started = true;
            } else {
                ssh_message_reply_default(message);
            }
            ssh_message_free(message);
        }
        if (!chan || !sftp_started) {
            if (chan) ssh_channel_close(chan);
            ssh_disconnect(session_);
            ssh_free(session_);
            session_ = nullptr;
            continue;
        }
        // 创建SFTP服务器（libssh只提供客户端API，这里可用sftp_server_new/sftp_server_init等扩展，或直接用openssh-sftp-server进程代理）
        // 这里建议直接fork/exec /usr/lib/ssh/sftp-server 或 /usr/libexec/sftp-server
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程，重定向输入输出到chan
            int in = ssh_channel_get_fd(chan);
            dup2(in, 0);
            dup2(in, 1);
            execlp("/usr/lib/ssh/sftp-server", "sftp-server", "-d", root_dir_.c_str(), nullptr);
            _exit(127);
        }
        // 父进程，等待子进程结束
        int status = 0;
        waitpid(pid, &status, 0);
        ssh_channel_close(chan);
        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = nullptr;
    }
    std::cout << "[SFTPServer] SFTP服务线程退出" << std::endl;
}