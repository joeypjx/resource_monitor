#include "sftp_client.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

SFTPClient::SFTPClient() {
    libssh2_init(0);
}

SFTPClient::~SFTPClient() {
    libssh2_exit();
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

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        err_msg = "无法创建套接字";
        return false;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(host.c_str());
    if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        err_msg = "无法连接到主机";
        close(sock);
        return false;
    }

    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        err_msg = "无法初始化libssh2会话";
        close(sock);
        return false;
    }

    libssh2_session_set_blocking(session, 1);

    if (libssh2_session_handshake(session, sock)) {
        err_msg = "SSH握手失败";
        libssh2_session_free(session);
        close(sock);
        return false;
    }

    if (libssh2_userauth_password(session, user.c_str(), pass.c_str())) {
        err_msg = "SSH认证失败";
        libssh2_session_disconnect(session, "Authentication failed");
        libssh2_session_free(session);
        close(sock);
        return false;
    }

    LIBSSH2_SFTP *sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        err_msg = "无法初始化SFTP会话";
        libssh2_session_disconnect(session, "SFTP init failed");
        libssh2_session_free(session);
        close(sock);
        return false;
    }

    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(sftp_session, remote_path.c_str(), LIBSSH2_FXF_READ, 0);
    if (!sftp_handle) {
        err_msg = "无法打开远程文件";
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "SFTP open failed");
        libssh2_session_free(session);
        close(sock);
        return false;
    }

    std::ofstream ofs(local_path, std::ios::binary);
    if (!ofs) {
        err_msg = "无法创建本地文件: " + local_path;
        libssh2_sftp_close(sftp_handle);
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "Local file creation failed");
        libssh2_session_free(session);
        close(sock);
        return false;
    }

    char buffer[4096];
    ssize_t nbytes;
    while ((nbytes = libssh2_sftp_read(sftp_handle, buffer, sizeof(buffer))) > 0) {
        ofs.write(buffer, nbytes);
    }

    if (nbytes < 0) {
        err_msg = "SFTP读取失败";
    }
    
    ofs.close();
    libssh2_sftp_close(sftp_handle);
    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    close(sock);

    return nbytes == 0;
} 