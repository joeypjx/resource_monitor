#!/bin/bash

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "请以 root 身份运行"
    exit 1
fi

# check if /usr/local/zygl exists
if [ ! -d /usr/local/zygl ]; then
    mkdir -p /usr/local/zygl
    echo "创建 /usr/local/zygl 目录"
fi

# check if agent exists
if [ -f agent ]; then
    cp ./agent /usr/local/zygl/agent
    chmod +x /usr/local/zygl/agent
    echo "agent 文件已复制到 /usr/local/zygl/agent"
fi

# check if /usr/local/zygl/agent exists
if [ ! -f /usr/local/zygl/agent ]; then
    echo "/usr/local/zygl/agent 不存在"
    exit 1
fi

# Prompt for manager IP
read -p "请输入 Manager 服务器 IP: " manager_ip

# check if manager is reachable
if ! ping -c 1 ${manager_ip} > /dev/null 2>&1; then
    echo "无法访问 Manager 服务器 ${manager_ip}"
    exit 1
fi

# Prompt for network interface (optional)
read -p "请输入网络接口名称 (可选, 按 Enter 自动检测): " network_interface

# Check if service already exists, replace it
if systemctl list-unit-files | grep -q "dtagent.service"; then
    echo "dtagent.service 已存在, 正在替换..."
    systemctl stop dtagent.service
    systemctl disable dtagent.service
    rm -f /etc/systemd/system/dtagent.service
fi

# Build the ExecStart command
exec_start_cmd="/usr/local/zygl/agent --manager-url http://${manager_ip}:38080 --port 38081"

# Add network interface parameter if provided
if [ ! -z "$network_interface" ]; then
    exec_start_cmd="${exec_start_cmd} --network-interface ${network_interface}"
fi

# Create service file
cat > /etc/systemd/system/dtagent.service << EOL
[Unit]
Description=DT Agent Service
After=network.target

[Service]
WorkingDirectory=/usr/local/zygl
Type=simple
ExecStart=${exec_start_cmd}
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOL

# Reload systemd and enable service
systemctl daemon-reload
systemctl enable dtagent.service
systemctl start dtagent.service

echo "dtagent.service 已创建并启用"
echo "您可以使用以下命令检查服务状态: systemctl status dtagent.service"