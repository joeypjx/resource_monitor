#ifndef SFTP_SERVER_H
#define SFTP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

/**
 * SFTPServer类 - 基于libssh的SFTP服务器
 * 支持指定根目录，提供二进制和镜像文件下载
 */
class SFTPServer {
public:
    SFTPServer(int port, const std::string& root_dir);
    ~SFTPServer();

    // 启动SFTP服务器
    bool start();
    // 停止SFTP服务器
    void stop();
    // 是否正在运行
    bool isRunning() const;

private:
    void serverThreadFunc();
    int port_;
    std::string root_dir_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    // libssh session等成员
    ssh_bind sshbind_ = nullptr;
    ssh_session session_ = nullptr;
    sftp_session sftp_ = nullptr;
};

#endif // SFTP_SERVER_H 