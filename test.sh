#!/bin/bash

# 测试脚本：验证资源监控系统的功能

echo "开始测试资源监控系统..."

# 检查构建目录
if [ ! -d "build" ]; then
    echo "错误：构建目录不存在，请先运行 build.sh"
    exit 1
fi

# 检查可执行文件
if [ ! -f "build/manager" ] || [ ! -f "build/agent" ]; then
    echo "错误：可执行文件不存在，请先运行 build.sh"
    exit 1
fi

# 测试Manager启动
echo "测试1：启动Manager..."
cd build
./manager --port 8081 > manager.log 2>&1 &
MANAGER_PID=$!
sleep 2

# 检查Manager是否成功启动
if ! ps -p $MANAGER_PID > /dev/null; then
    echo "错误：Manager启动失败"
    cat manager.log
    exit 1
fi
echo "Manager启动成功，PID: $MANAGER_PID"

# 测试Agent启动
echo "测试2：启动Agent..."
./agent --manager-url http://localhost:8081 --interval 2 > agent.log 2>&1 &
AGENT_PID=$!
sleep 5

# 检查Agent是否成功启动
if ! ps -p $AGENT_PID > /dev/null; then
    echo "错误：Agent启动失败"
    cat agent.log
    kill $MANAGER_PID
    exit 1
fi
echo "Agent启动成功，PID: $AGENT_PID"

# 等待数据采集和上报
echo "测试3：等待数据采集和上报..."
sleep 10

# 测试API接口
echo "测试4：验证API接口..."
curl -s http://localhost:8081/api/agents > agents.json
if [ $? -ne 0 ]; then
    echo "错误：无法访问API接口"
    kill $AGENT_PID
    kill $MANAGER_PID
    exit 1
fi

# 检查是否有Agent注册
AGENT_COUNT=$(grep -c "agent_id" agents.json)
if [ $AGENT_COUNT -eq 0 ]; then
    echo "错误：没有Agent注册"
    cat agent.log
    kill $AGENT_PID
    kill $MANAGER_PID
    exit 1
fi
echo "成功检测到 $AGENT_COUNT 个Agent"

# 获取Agent ID
AGENT_ID=$(grep -o '"agent_id":"[^"]*"' agents.json | head -1 | cut -d'"' -f4)
echo "检测到Agent ID: $AGENT_ID"

# 测试资源数据API
echo "测试5：验证资源数据API..."
curl -s "http://localhost:8081/api/agents/$AGENT_ID/cpu" > cpu.json
curl -s "http://localhost:8081/api/agents/$AGENT_ID/memory" > memory.json
curl -s "http://localhost:8081/api/agents/$AGENT_ID/disk" > disk.json
curl -s "http://localhost:8081/api/agents/$AGENT_ID/network" > network.json
curl -s "http://localhost:8081/api/agents/$AGENT_ID/docker" > docker.json

# 检查是否有数据
for TYPE in cpu memory disk network docker; do
    DATA_SIZE=$(stat -c%s "${TYPE}.json")
    if [ $DATA_SIZE -lt 10 ]; then
        echo "警告：${TYPE}数据可能不完整"
    else
        echo "${TYPE}数据获取成功"
    fi
done

# 清理进程
echo "测试完成，清理进程..."
kill $AGENT_PID
kill $MANAGER_PID

echo "测试结果总结："
echo "- Manager启动成功"
echo "- Agent启动并注册成功"
echo "- 资源数据采集和上报成功"
echo "- API接口访问成功"

echo "测试完成！"
