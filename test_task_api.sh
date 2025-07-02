#!/bin/bash

# 检查 jq 是否已安装
if ! command -v jq &> /dev/null
then
    echo "未找到 jq 命令。请安装 jq 后再运行此脚本。"
    echo "macOS: brew install jq"
    echo "Debian/Ubuntu: sudo apt-get install jq"
    exit
fi

BASE_URL="http://127.0.0.1:8080"
TASK_GROUP_NAME="我的API测试任务"

# 函数：打印分隔线和标题
print_header() {
    echo ""
    echo "----------------------------------------------------"
    echo "$1"
    echo "----------------------------------------------------"
}

echo "===================================================="
echo "开始任务管理API测试"
echo "===================================================="
echo "基本URL: $BASE_URL"


# 1. 查询节点列表
print_header "1. 测试: 查询节点列表 (GET /api/task/node)"
echo "响应:"
curl -s -X GET "${BASE_URL}/api/task/node" | jq .


# 2. 创建任务组模板
print_header "2. 测试: 创建任务组模板 (POST /api/task/task_group)"
CREATE_PAYLOAD=$(cat <<EOF
{
  "name": "$TASK_GROUP_NAME",
  "task_groups": [
    {
      "tasks": [
        {
          "name": "休眠任务1",
          "config": {
            "command": "/root/sleep"
          }
        },
        {
          "name": "休眠任务2",
          "config": {
            "command": "/root/sleep"
          }
        },
        {
          "name": "休眠任务3",
          "config": {
            "command": "/root/sleep"
          }
        }
      ]
    }
  ]
}
EOF
)

echo "请求体:"
echo "$CREATE_PAYLOAD" | jq .
echo ""
echo "响应:"
CREATE_RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "${BASE_URL}/api/task/task_group" \
-H "Content-Type: application/json" \
-d "$CREATE_PAYLOAD")

CREATE_HTTP_CODE=$(echo "$CREATE_RESPONSE" | tail -n1)
CREATE_BODY=$(echo "$CREATE_RESPONSE" | sed '$d')
echo "$CREATE_BODY" | jq .
echo "HTTP Status Code: $CREATE_HTTP_CODE"

if [ "$CREATE_HTTP_CODE" -ne 200 ] || [ "$(echo $CREATE_BODY | jq -r .status)" != "success" ]; then
    echo "创建任务组模板失败，测试终止。"
    exit 1
fi


# 3. 查询任务组模板
print_header "3. 测试: 查询任务组模板 (POST /api/task/query)"
QUERY_PAYLOAD=$(cat <<EOF
{
  "name": "$TASK_GROUP_NAME"
}
EOF
)
echo "请求体:"
echo "$QUERY_PAYLOAD" | jq .
echo ""
echo "响应:"
curl -s -X POST "${BASE_URL}/api/task/query" \
-H "Content-Type: application/json" \
-d "$QUERY_PAYLOAD" | jq .


# 4. 部署任务组
print_header "4. 测试: 部署任务组 (POST /api/task/task_group/deploy)"
DEPLOY_PAYLOAD=$(cat <<EOF
{
  "name": "$TASK_GROUP_NAME"
}
EOF
)
echo "请求体:"
echo "$DEPLOY_PAYLOAD" | jq .
echo ""
echo "响应:"
DEPLOY_RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "${BASE_URL}/api/task/task_group/deploy" \
-H "Content-Type: application/json" \
-d "$DEPLOY_PAYLOAD")

DEPLOY_HTTP_CODE=$(echo "$DEPLOY_RESPONSE" | tail -n1)
DEPLOY_BODY=$(echo "$DEPLOY_RESPONSE" | sed '$d')

echo "$DEPLOY_BODY" | jq .
echo "HTTP Status Code: $DEPLOY_HTTP_CODE"

BUSINESS_ID=$(echo "$DEPLOY_BODY" | jq -r '.id')

if [ "$DEPLOY_HTTP_CODE" -ne 200 ] || [ -z "$BUSINESS_ID" ] || [ "$BUSINESS_ID" == "null" ]; then
    echo "部署任务组失败或未能获取ID，测试终止。"
    exit 1
fi

echo "提取到的任务组实例ID: $BUSINESS_ID"


# 等待部署生效
echo "等待5秒以便任务组部署..."
sleep 5


# 5. 查询任务组状态
print_header "5. 测试: 查询任务组状态 (POST /api/task/task_group/query)"
STATUS_PAYLOAD=$(cat <<EOF
{
  "id": "$BUSINESS_ID"
}
EOF
)
echo "请求体:"
echo "$STATUS_PAYLOAD" | jq .
echo ""
echo "响应:"
curl -s -X POST "${BASE_URL}/api/task/task_group/query" \
-H "Content-Type: application/json" \
-d "$STATUS_PAYLOAD" | jq .


# 等待一段时间再停止
echo "等待5秒..."
sleep 5


# 6. 停止任务组
print_header "6. 测试: 停止任务组 (POST /api/task/task_group/stop)"
STOP_PAYLOAD=$(cat <<EOF
{
  "id": "$BUSINESS_ID"
}
EOF
)
echo "请求体:"
echo "$STOP_PAYLOAD" | jq .
echo ""
echo "响应:"
curl -s -X POST "${BASE_URL}/api/task/task_group/stop" \
-H "Content-Type: application/json" \
-d "$STOP_PAYLOAD" | jq .

echo ""
echo "===================================================="
echo "API测试完成。"
echo "====================================================" 