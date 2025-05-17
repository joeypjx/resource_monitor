#!/bin/bash

# 资源监控系统构建脚本
# 使用Makefile替代CMake构建

set -e

echo "===== 资源监控系统构建脚本 ====="
echo "使用Makefile替代CMake构建"
echo ""

# 检查依赖
echo "正在检查依赖..."
make deps || { echo "依赖检查失败，请安装所需依赖"; exit 1; }

# 清理旧的构建文件
if [ -d "build" ]; then
    echo "清理旧的构建文件..."
    make clean
fi

# 构建项目
echo "开始构建项目..."
make all

echo ""
echo "构建完成！可执行文件位于 build/ 目录"
echo "- Agent: build/agent"
echo "- Manager: build/manager"
echo ""
echo "运行示例:"
echo "  ./build/manager --port 8080 --db-path resource_monitor.db"
echo "  ./build/agent --manager-url http://localhost:8080 --interval 5"
echo ""
echo "更多信息请查看 README_MAKEFILE.md"