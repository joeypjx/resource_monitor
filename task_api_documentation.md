# 任务管理 (Task Management)

本文档详细描述了与任务管理相关的API接口。

---

## 1. 创建任务组模板

**描述**：定义一个新的任务组模板，该模板包含一个名称和一组任务配置。此模板可用于后续的任务部署。

**接口**：`POST /api/task/task_group`

**输入 (Input)**：
```json
{
  "name": "我的数据处理任务",
  "task_groups": [
    {
      "tasks": [
        {
          "name": "数据清洗器",
          "config": {
            "command": "/opt/apps/clean.sh"
          }
        },
        {
          "name": "数据分析器",
          "config": {
            "command": "/opt/apps/analyze.py"
          }
        }
      ]
    }
  ]
}
```

**响应 (Output)**：
```json
{
  "status": "success"
}
```

---

## 2. 查询任务组模板

**描述**：根据名称查询一个已创建的任务组模板的详细信息。

**接口**：`POST /api/task/query`

**输入 (Input)**：
```json
{
  "name": "我的数据处理任务"
}
```

**响应 (Output)**：
```json
{
  "name": "我的数据处理任务",
  "task_groups": [
    {
      "tasks": [
        {
          "name": "数据清洗器",
          "config": {
            "command": "/opt/apps/clean.sh"
          }
        },
        {
          "name": "数据分析器",
          "config": {
            "command": "/opt/apps/analyze.py"
          }
        }
      ]
    }
  ]
}
```

---

## 3. 部署任务组

**描述**：根据指定的任务组模板名称，部署一个新的任务组实例。

**接口**：`POST /api/task/task_group/deploy`

**输入 (Input)**：
```json
{
  "name": "我的数据处理任务"
}
```

**响应 (Output)**：
```json
{
  "id": "generated-business-instance-uuid"
}
```

---

## 4. 查询任务组状态

**描述**：根据任务组实例ID查询其运行状态。

**接口**：`POST /api/task/task_group/query`

**输入 (Input)**：
```json
{
  "id": "business-instance-uuid"
}
```

**响应 (Output)**：
```json
{
  "id": "business-instance-uuid",
  "status": 0
}
```
**注意**: `status` 字段中, `0` 代表正在运行, `1` 代表已停止或存在异常。

---

## 5. 停止任务组

**描述**：根据任务组实例ID停止并删除一个正在运行的任务组。

**接口**：`POST /api/task/task_group/stop`

**输入 (Input)**：
```json
{
  "id": "business-instance-uuid"
}
```

**响应 (Output)**：
```json
{
  "id": "business-instance-uuid"
}
```

---

# 节点管理 (Node Management)

## 1. 查询节点列表

**描述**：获取所有已注册的计算节点及其当前的资源使用情况。

**接口**：`GET /api/task/node`

**输入 (Input)**：无

**响应 (Output)**：
```json
{
  "nodes": [
    {
      "ip": "192.168.1.100",
      "status": 0,
      "resources": "cpu_usage_percent:50,memory_usage_percent:30"
    },
    {
      "ip": "192.168.1.101",
      "status": 1,
      "resources": "cpu_usage_percent:0,memory_usage_percent:0"
    }
  ]
}
```
**注意**: `status` 字段中, `0` 代表节点在线, `1` 代表节点离线。 