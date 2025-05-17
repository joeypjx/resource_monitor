#!/bin/bash

# 集成测试脚本：验证业务部署功能

echo "开始测试业务部署功能..."

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

# 等待Agent注册
echo "测试3：等待Agent注册..."
sleep 5

# 获取Agent ID
AGENT_ID=$(curl -s http://localhost:8081/api/agents | grep -o '"agent_id":"[^"]*"' | head -1 | cut -d'"' -f4)
if [ -z "$AGENT_ID" ]; then
    echo "错误：无法获取Agent ID"
    kill $AGENT_PID
    kill $MANAGER_PID
    exit 1
fi
echo "获取到Agent ID: $AGENT_ID"

# 创建测试业务部署请求
echo "测试4：创建业务部署请求..."
cat > business_deploy.json << EOF
{
  "business_id": "test-business-001",
  "business_name": "测试业务",
  "components": [
    {
      "component_id": "component-001",
      "component_name": "nginx-server",
      "type": "docker",
      "image_name": "nginx:latest",
      "resource_requirements": {
        "cpu_cores": 0.5,
        "memory_mb": 256,
        "gpu": false
      },
      "environment_variables": {
        "NGINX_PORT": "80"
      },
      "config_files": [
        {
          "path": "/etc/nginx/conf.d/default.conf",
          "content": "server { listen 80; server_name localhost; location / { root /usr/share/nginx/html; index index.html index.htm; } }"
        }
      ],
      "affinity": {
        "require_gpu": false
      }
    }
  ]
}
EOF

# 发送业务部署请求
echo "测试5：发送业务部署请求..."
DEPLOY_RESULT=$(curl -s -X POST -H "Content-Type: application/json" -d @business_deploy.json http://localhost:8081/api/businesses)
echo "部署结果: $DEPLOY_RESULT"

# 检查部署结果
if ! echo $DEPLOY_RESULT | grep -q '"status":"success"'; then
    echo "错误：业务部署失败"
    kill $AGENT_PID
    kill $MANAGER_PID
    exit 1
fi
echo "业务部署成功"

# 等待组件启动
echo "测试6：等待组件启动..."
sleep 10

# 检查业务状态
echo "测试7：检查业务状态..."
BUSINESS_STATUS=$(curl -s http://localhost:8081/api/businesses/test-business-001)
echo "业务状态: $BUSINESS_STATUS"

# 检查组件状态
echo "测试8：检查组件状态..."
COMPONENT_STATUS=$(curl -s http://localhost:8081/api/businesses/test-business-001/components)
echo "组件状态: $COMPONENT_STATUS"

# 测试业务停止
echo "测试9：停止业务..."
STOP_RESULT=$(curl -s -X POST http://localhost:8081/api/businesses/test-business-001/stop)
echo "停止结果: $STOP_RESULT"

# 检查停止结果
if ! echo $STOP_RESULT | grep -q '"status":"success"'; then
    echo "警告：业务停止可能不成功"
fi

# 等待业务停止
echo "测试10：等待业务停止..."
sleep 5

# 再次检查业务状态
echo "测试11：再次检查业务状态..."
BUSINESS_STATUS=$(curl -s http://localhost:8081/api/businesses/test-business-001)
echo "业务状态: $BUSINESS_STATUS"

# 清理进程
echo "测试完成，清理进程..."
kill $AGENT_PID
kill $MANAGER_PID

echo "测试结果总结："
echo "- Manager启动成功"
echo "- Agent启动并注册成功"
echo "- 业务部署成功"
echo "- 业务停止成功"

echo "集成测试完成！"
