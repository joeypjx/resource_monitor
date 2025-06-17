#!/bin/bash

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root"
    exit 1
fi

# check if /usr/local/zygl/manager exists
if [ ! -f /usr/local/zygl/manager ]; then
    echo "/usr/local/zygl/manager does not exist"
    exit 1
fi

# Prompt for SFTP credentials
read -p "Enter SFTP server IP: " sftp_ip
read -s -p "Enter SFTP root password: " sftp_password
echo

# check sftp server is reachable
if ! ping -c 1 ${sftp_ip} > /dev/null 2>&1; then
    echo "sftp server ${sftp_ip} is not reachable"
    exit 1
fi

# Check if service already exists replace it
if systemctl list-unit-files | grep -q "dtmanager.service"; then
    echo "dtmanager.service already exists, replacing it"
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

echo "dtmanager.service has been created and enabled"
echo "You can check the status with: systemctl status dtmanager.service"