#!/bin/bash

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "请以 root 身份运行"
    exit 1
fi

# check if /usr/local/zygl/manager exists
if [ ! -f /usr/local/zygl/manager ]; then
    echo "/usr/local/zygl/manager 不存在"
    exit 1
fi

# Prompt for SFTP credentials
read -p "请输入 SFTP 服务器 IP: " sftp_ip
read -s -p "请输入 SFTP root 密码: " sftp_password
echo

# check sftp server is reachable
if ! ping -c 1 ${sftp_ip} > /dev/null 2>&1; then
    echo "无法访问 SFTP 服务器 ${sftp_ip}"
    exit 1
fi

# Check if service already exists replace it
if systemctl list-unit-files | grep -q "dtmanager.service"; then
    echo "dtmanager.service 已存在, 正在替换..."
    systemctl stop dtmanager.service
    systemctl disable dtmanager.service
    rm -f /etc/systemd/system/dtmanager.service
fi

# Create service file
cat > /etc/systemd/system/dtmanager.service << EOL
[Unit]
Description=DT Manager Service
After=network.target

[Service]
WorkingDirectory=/usr/local/zygl
Type=simple
ExecStart=/usr/local/zygl/manager --port 38080 --db-path /usr/local/zygl/manager.db --sftp-host sftp://root:${sftp_password}@${sftp_ip}:22/data/zygl/
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOL

# Reload systemd and enable service
systemctl daemon-reload
systemctl enable dtmanager.service
systemctl start dtmanager.service

echo "dtmanager.service 已创建并启用"
echo "您可以使用以下命令检查服务状态: systemctl status dtmanager.service"