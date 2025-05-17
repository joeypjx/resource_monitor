#! /bin/bash

# 创建构建目录
mkdir -p build
cd build

# 运行CMake配置
cmake ..

# 编译
make -j$(nproc)

echo "构建完成！"
echo "使用以下命令运行Manager: ./manager"
echo "使用以下命令运行Agent: ./agent --manager-url http://localhost:8080"
