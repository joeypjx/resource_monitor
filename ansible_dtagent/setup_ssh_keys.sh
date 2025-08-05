#!/bin/bash

# ==============================================================================
#  SSH Key Distribution Script for Ansible
#
#  这个脚本会自动执行以下操作:
#  1. 检查本地是否存在 SSH 密钥对，如果不存在则生成。
#  2. 读取一个指定的 inventory 文件 (与 Ansible 格式兼容)。
#  3. 提示输入要登录的用户名。
#  4. 使用 ssh-copy-id 将公钥分发到 inventory 中的所有服务器。
# ==============================================================================

# --- 配置 ---
# Ansible inventory 文件的路径
INVENTORY_FILE="inventory.ini"
# 公钥文件的路径
PUB_KEY_PATH="${HOME}/.ssh/id_rsa.pub"

# --- 颜色定义 ---
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# --- 脚本开始 ---

echo -e "${GREEN}### 开始为 Ansible 准备 SSH 免密登录 ###${NC}"

# 1. 检查并生成 SSH 密钥
if [ ! -f "$PUB_KEY_PATH" ]; then
    echo -e "${YELLOW}未找到 SSH 公钥，正在生成新的密钥对...${NC}"
    ssh-keygen -t rsa -b 4096 -f "${HOME}/.ssh/id_rsa" -N ""
    if [ $? -ne 0 ]; then
        echo -e "${RED}错误：SSH 密钥生成失败。请检查权限并重试。${NC}"
        exit 1
    fi
    echo -e "${GREEN}成功生成 SSH 密钥: ${PUB_KEY_PATH}${NC}"
else
    echo -e "发现已存在的 SSH 公钥: ${PUB_KEY_PATH}"
fi

# 2. 检查 inventory 文件是否存在
if [ ! -f "$INVENTORY_FILE" ]; then
    echo -e "${RED}错误: Inventory 文件 '${INVENTORY_FILE}' 不存在。${NC}"
    echo "请确保脚本与 inventory.ini 文件在同一目录下，或者修改脚本中的 INVENTORY_FILE 变量。"
    exit 1
fi

# 3. 提示输入远程服务器的用户名
read -p "请输入你要在远程服务器上使用的用户名 (例如: root): " SSH_USER
if [ -z "$SSH_USER" ]; then
    echo -e "${RED}错误：用户名不能为空。${NC}"
    exit 1
fi

echo -e "\n${GREEN}准备将公钥分发到以下服务器:${NC}"

# 提取服务器列表 (忽略注释和分组标题)
HOSTS=$(grep -vE '^\s*#|^\s*$' "$INVENTORY_FILE" | grep -v '\[.*\]' | grep -v '=')
echo "$HOSTS"
echo "----------------------------------------"

# 4. 循环遍历服务器并分发公钥
for host in $HOSTS; do
    echo -e "\n${YELLOW}>>> 正在处理服务器: ${host}${NC}"
    ssh-copy-id -i "$PUB_KEY_PATH" "${SSH_USER}@${host}"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}成功将密钥复制到 ${host}${NC}"
    else
        echo -e "${RED}无法将密钥复制到 ${host}。请检查网络连接、主机名和密码。${NC}"
    fi
done

echo -e "\n${GREEN}### 所有服务器处理完毕！ ###${NC}"
echo "你现在应该可以从这台机器免密登录到目标服务器了。"
echo "可以运行 ansible -i inventory.ini all -m ping 来测试连接。"