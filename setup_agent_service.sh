#!/bin/bash

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root"
    exit 1
fi

# check if /usr/local/zygl/agent exists
if [ ! -f /usr/local/zygl/agent ]; then
    echo "/usr/local/zygl/agent does not exist"
    exit 1
fi


# Prompt for manager IP
read -p "Enter manager server IP: " manager_ip

# check if manager is reachable
if ! ping -c 1 ${manager_ip} > /dev/null 2>&1; then
    echo "manager ${manager_ip} is not reachable"
    exit 1
fi

# Check if service already exists, replace it
if systemctl list-unit-files | grep -q "dtagent.service"; then
    echo "dtagent.service already exists, replacing it"
    systemctl stop dtagent.service
    systemctl disable dtagent.service
    rm -f /etc/systemd/system/dtagent.service
fi

# Create service file
cat > /etc/systemd/system/dtagent.service << EOL
[Unit]
Description=DT Agent Service
After=network.target

[Service]
WorkingDirectory=/usr/local/zygl
Type=simple
ExecStart=/usr/local/zygl/agent --manager-url http://${manager_ip}:38080 --port 38081
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOL

# Reload systemd and enable service
systemctl daemon-reload
systemctl enable dtagent.service
systemctl start dtagent.service

echo "dtagent.service has been created and enabled"
echo "You can check the status with: systemctl status dtagent.service"