#include "sftp_client.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <fcntl.h>

SFTPClient::SFTPClient() {
}

SFTPClient::~SFTPClient() {
}

bool SFTPClient::parseUrl(const std::string& url, std::string& user, std::string& pass, std::string& host, int& port, std::string& remote_path) {
    // sftp://user:pass@host:port/path
    std::regex re_sftp(R"(sftp://([^:]+):([^@]+)@([^/:]+)(:(\d+))?(/.+))");
    std::smatch match;
    if (std::regex_match(url, match, re_sftp)) {
        user = match[1];
        pass = match[2];
        host = match[3];
        port = match[5].matched ? std::stoi(match[5]) : 22;
        remote_path = match[6];
        return true;
    }
    return false;
}

bool SFTPClient::downloadFile(const std::string& url, const std::string& local_path, std::string& err_msg) {
    std::string user = "", pass = "", host = "", remote_path = "";
    int port = 22;
    if (!parseUrl(url, user, pass, host, port, remote_path)) {
        err_msg = "SFTP URL格式错误";
        return false;
    }

    ssh_session session = ssh_new();
    if (session == nullptr) {
        err_msg = "无法创建SSH会话";
        return false;
    }

    ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str());
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, user.c_str());

    int rc = ssh_connect(session);
    if (rc != SSH_OK) {
        err_msg = "无法连接到主机: " + std::string(ssh_get_error(session));
        ssh_free(session);
        return false;
    }

    rc = ssh_userauth_password(session, nullptr, pass.c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        err_msg = "SSH认证失败: " + std::string(ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        return false;
    }

    sftp_session sftp = sftp_new(session);
    if (sftp == nullptr) {
        err_msg = "无法创建SFTP会话";
        ssh_disconnect(session);
        ssh_free(session);
        return false;
    }

    rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        err_msg = "无法初始化SFTP会话: " + std::string(ssh_get_error(session));
        sftp_free(sftp);
        ssh_disconnect(session);
        ssh_free(session);
        return false;
    }

    sftp_file file = sftp_open(sftp, remote_path.c_str(), O_RDONLY, 0);
    if (file == nullptr) {
        err_msg = "无法打开远程文件: " + std::string(ssh_get_error(session));
        sftp_free(sftp);
        ssh_disconnect(session);
        ssh_free(session);
        return false;
    }

    std::ofstream ofs(local_path, std::ios::binary);
    if (!ofs) {
        err_msg = "无法创建本地文件: " + local_path;
        sftp_close(file);
        sftp_free(sftp);
        ssh_disconnect(session);
        ssh_free(session);
        return false;
    }

    char buffer[4096];
    ssize_t nbytes;
    while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        ofs.write(buffer, nbytes);
    }
    
    bool success = true;
    if (nbytes < 0) {
        err_msg = "SFTP读取失败: " + std::string(ssh_get_error(session));
        success = false;
    }
    
    ofs.close();
    sftp_close(file);
    sftp_free(sftp);
    ssh_disconnect(session);
    ssh_free(session);

    return success;
} 