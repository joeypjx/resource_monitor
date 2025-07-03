#ifndef SFTP_CLIENT_H
#define SFTP_CLIENT_H

#include <string>
#include <libssh2.h>
#include <libssh2_sftp.h>

// SFTPClient类 - 基于libssh2的SFTP客户端
// 支持通过SFTP协议下载文件
class SFTPClient {
public:
    SFTPClient();
    ~SFTPClient();

    // 下载文件
    // @param url sftp://user:pass@host:port/path/to/file
    // @param local_path 本地保存路径
    // @return 是否成功
    bool downloadFile(const std::string& url, const std::string& local_path, std::string& err_msg);

private:
    // 解析SFTP URL
    bool parseUrl(const std::string& url, std::string& user, std::string& pass, std::string& host, int& port, std::string& remote_path);
};

#endif // SFTP_CLIENT_H 