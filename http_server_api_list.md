# HTTPServer API 列表与数据格式

## 业务管理相关

### 1. 部署业务
- **POST** `/api/businesses`
- **请求体字段说明**：
  - `business_name` (string): 业务名称
  - `components` (array): 组件列表，每个组件包含如下字段：
    - `component_id` (string): 组件ID
    - `component_name` (string): 组件名称
    - `type` (string): 组件类型（如 "docker"、"binary"）
    - `affinity` (object, 可选): 亲和性约束
    - `image_url` (string, docker类型时): 镜像URL
    - `image_name` (string, docker类型时): 镜像名
    - `binary_path` (string, binary类型时): 二进制路径
    - `binary_url` (string, binary类型时): 二进制下载链接
    - `environment_variables` (object, 可选): 环境变量
- **请求体示例**：
```json
{
  "business_name": "AI推理服务",
  "components": [
    {
      "component_id": "ct-xxxxxxxxxx",
      "component_name": "推理引擎",
      "type": "docker",
      "image_url": "registry.example.com/ai-infer:latest",
      "image_name": "ai-infer",
      "affinity": {
        "hostname": "node-1"
      },
      "environment_variables": {
        "MODEL_PATH": "/models/model1"
      }
    }
  ]
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
  - `business_id` (string): 新建业务ID
- **响应示例**：
```json
{
  "status": "success",
  "message": "Business deployed successfully",
  "business_id": "b-123456"
}
```

### 2. 停止业务
- **POST** `/api/businesses/:business_id/stop`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Business stopped successfully"
}
```

### 3. 重启业务
- **POST** `/api/businesses/:business_id/restart`
- **响应**：同上

### 4. 更新业务
- **PUT** `/api/businesses/:business_id`
- **请求体字段说明**：同"部署业务"
- **响应**：同"部署业务"

### 5. 获取业务列表
- **GET** `/api/businesses`
- **响应字段说明**：
  - `status` (string): "success"
  - `result` (array): 业务对象数组
- **业务对象示例**：
```json
{
  "business_id": "b-123456",
  "business_name": "AI推理服务",
  "status": "running",
  "components": [
    {
      "component_id": "c-1",
      "component_name": "推理引擎",
      "type": "docker",
      "status": "running"
    }
  ],
  "created_at": 1710000000,
  "updated_at": 1710000000
}
```
- **响应示例**：
```json
{
  "status": "success",
  "result": [ /* 业务对象数组 */ ]
}
```

### 6. 获取业务详情
- **GET** `/api/businesses/:business_id`
- **响应字段说明**：同上，返回单个业务对象
- **响应示例**：
```json
{
  "status": "success",
  "result": {
    "business_id": "b-123456",
    "business_name": "AI推理服务",
    "status": "running",
    "components": [ /* 组件对象数组 */ ],
    "created_at": 1710000000,
    "updated_at": 1710000000
  }
}
```

### 7. 获取业务组件
- **GET** `/api/businesses/:business_id/components`
- **响应字段说明**：
  - `status` (string): "success"
  - `components` (array): 组件对象数组
- **组件对象示例**：
```json
{
  "component_id": "c-1",
  "component_name": "推理引擎",
  "type": "docker",
  "image_url": "registry.example.com/ai-infer:latest",
  "image_name": "ai-infer",
  "affinity": { "hostname": "node-1" },
  "environment_variables": { "MODEL_PATH": "/models/model1" },
  "node_id": "node-1",
  "container_id": "container-xxx",
  "status": "running",
  "started_at": 1710000000,
  "updated_at": 1710000000
}
```
- **响应示例**：
```json
{
  "status": "success",
  "components": [ /* 组件对象数组 */ ]
}
```

### 8. 组件状态上报
- **POST** `/api/report/component`
- **请求体字段说明**：
  - `component_id` (string): 组件ID
  - `business_id` (string): 业务ID
  - `status` (string): 组件状态（如 running/stopped/error）
  - `container_id` (string, 可选): 容器ID
  - `process_id` (string, 可选): 进程ID
- **请求体示例**：
```json
{
  "component_id": "c-1",
  "business_id": "b-123456",
  "status": "running",
  "container_id": "container-xxx"
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Component status updated"
}
```

---

## 模板管理相关

### 1. 创建组件模板
- **POST** `/api/templates/components`
- **请求体字段说明**：
  - `template_name` (string): 模板名称
  - `type` (string): 组件类型（如 "docker"、"binary"）
  - `description` (string, 可选): 模板描述
  - `config` (object): 组件配置参数（结构自定义）
- **请求体示例**：
```json
{
  "template_name": "推理引擎模板",
  "type": "docker",
  "description": "AI推理服务的docker组件模板",
  "config": {
    "image_url": "registry.example.com/ai-infer:latest",
    "image_name": "ai-infer",
    "environment_variables": {
      "MODEL_PATH": "/models/default"
    }
  }
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `component_template_id` (string): 模板ID
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "component_template_id": "ct-xxxx",
  "message": "Component template created successfully"
}
```

### 2. 获取组件模板列表
- **GET** `/api/templates/components`
- **响应字段说明**：
  - `status` (string): "success"
  - `templates` (array): 组件模板对象数组
- **组件模板对象示例**：
```json
{
  "component_template_id": "ct-xxxx",
  "template_name": "推理引擎模板",
  "type": "docker",
  "description": "AI推理服务的docker组件模板",
  "config": {
    "image_url": "registry.example.com/ai-infer:latest",
    "image_name": "ai-infer",
    "environment_variables": {
      "MODEL_PATH": "/models/default"
    }
  },
  "created_at": "2024-06-01 12:00:00",
  "updated_at": "2024-06-01 12:00:00"
}
```
- **响应示例**：
```json
{
  "status": "success",
  "templates": [ /* 组件模板对象数组 */ ]
}
```

### 3. 获取组件模板详情
- **GET** `/api/templates/components/:template_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `template` (object): 组件模板对象
- **响应示例**：
```json
{
  "status": "success",
  "template": {
    "component_template_id": "ct-xxxx",
    "template_name": "推理引擎模板",
    "type": "docker",
    "description": "AI推理服务的docker组件模板",
    "config": {
      "image_url": "registry.example.com/ai-infer:latest",
      "image_name": "ai-infer",
      "environment_variables": {
        "MODEL_PATH": "/models/default"
      }
    },
    "created_at": "2024-06-01 12:00:00",
    "updated_at": "2024-06-01 12:00:00"
  }
}
```

### 4. 更新组件模板
- **PUT** `/api/templates/components/:template_id`
- **请求体字段说明**：同"创建组件模板"，需包含`component_template_id`
- **响应**：同"创建组件模板"

### 5. 删除组件模板
- **DELETE** `/api/templates/components/:template_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Component template deleted successfully"
}
```

### 6. 创建业务模板
- **POST** `/api/templates/businesses`
- **请求体字段说明**：
  - `template_name` (string): 业务模板名称
  - `description` (string, 可选): 模板描述
  - `components` (array): 组件模板引用数组，每项包含：
    - `component_template_id` (string): 组件模板ID
    - 其它自定义参数
- **请求体示例**：
```json
{
  "template_name": "AI推理业务模板",
  "description": "AI推理业务的标准模板",
  "components": [
    {
      "component_template_id": "ct-xxxx"
    }
  ]
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `business_template_id` (string): 业务模板ID
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "business_template_id": "bt-xxxx",
  "message": "Business template created successfully"
}
```

### 7. 获取业务模板列表
- **GET** `/api/templates/businesses`
- **响应字段说明**：
  - `status` (string): "success"
  - `templates` (array): 业务模板对象数组
- **业务模板对象示例**：
```json
{
  "business_template_id": "bt-xxxx",
  "template_name": "AI推理业务模板",
  "description": "AI推理业务的标准模板",
  "components": [
    {
      "component_template_id": "ct-xxxx",
      "template_details": { /* 组件模板详情 */ }
    }
  ],
  "created_at": "2024-06-01 12:00:00",
  "updated_at": "2024-06-01 12:00:00"
}
```
- **响应示例**：
```json
{
  "status": "success",
  "templates": [ /* 业务模板对象数组 */ ]
}
```

### 8. 获取业务模板详情
- **GET** `/api/templates/businesses/:template_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `template` (object): 业务模板对象
- **响应示例**：
```json
{
  "status": "success",
  "template": {
    "business_template_id": "bt-xxxx",
    "template_name": "AI推理业务模板",
    "description": "AI推理业务的标准模板",
    "components": [
      {
        "component_template_id": "ct-xxxx",
        "template_details": { /* 组件模板详情 */ }
      }
    ],
    "created_at": "2024-06-01 12:00:00",
    "updated_at": "2024-06-01 12:00:00"
  }
}
```

### 9. 更新业务模板
- **PUT** `/api/templates/businesses/:template_id`
- **请求体字段说明**：同"创建业务模板"，需包含`business_template_id`
- **响应**：同"创建业务模板"

### 10. 删除业务模板
- **DELETE** `/api/templates/businesses/:template_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Business template deleted successfully"
}
```

### 11. 业务模板转业务
- **GET** `/api/templates/businesses/:template_id/as-business`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `business` (object): 业务部署格式对象
- **业务对象示例**：
```json
{
  "business_name": "AI推理业务模板",
  "components": [
    {
      "type": "docker",
      "component_id": "ct-xxxx",
      "component_name": "推理引擎模板",
      "image_url": "registry.example.com/ai-infer:latest",
      "image_name": "ai-infer",
      "MODEL_PATH": "/models/default"
    }
  ]
}
```
- **响应示例**：
```json
{
  "status": "success",
  "business": { /* 业务对象 */ }
}
```

---

## 节点/板卡管理相关

### 1. 注册板卡
- **POST** `/api/register`
- **请求体字段说明**：
  - `board_id` (string, 可选): 板卡ID，不填则自动生成
  - `hostname` (string): 主机名
  - `ip_address` (string): IP地址
  - `os_info` (string): 操作系统信息
- **请求体示例**：
```json
{
  "hostname": "node-1",
  "ip_address": "192.168.1.100",
  "os_info": "Ubuntu 22.04"
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `board_id` (string): 板卡ID
- **响应示例**：
```json
{
  "status": "success",
  "board_id": "board-xxxx"
}
```

### 2. 板卡心跳
- **POST** `/api/heartbeat/:board_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Heartbeat updated"
}
```

### 3. 资源上报
- **POST** `/api/report`
- **请求体字段说明**：
  - `board_id` (string): 板卡ID
  - `timestamp` (int): 上报时间戳（秒）
  - `resource` (object): 资源数据
    - `cpu` (object): CPU资源
      - `usage_percent` (float): CPU使用率
      - `load_avg_1m` (float): 1分钟负载
      - `load_avg_5m` (float): 5分钟负载
      - `load_avg_15m` (float): 15分钟负载
      - `core_count` (int): CPU核心数
    - `memory` (object): 内存资源
      - `total` (int): 总内存（字节）
      - `used` (int): 已用内存（字节）
      - `free` (int): 空闲内存（字节）
      - `usage_percent` (float): 内存使用率
- **请求体示例**：
```json
{
  "board_id": "board-xxxx",
  "timestamp": 1710000000,
  "resource": {
    "cpu": {
      "usage_percent": 23.5,
      "load_avg_1m": 0.5,
      "load_avg_5m": 0.4,
      "load_avg_15m": 0.3,
      "core_count": 8
    },
    "memory": {
      "total": 16777216,
      "used": 8388608,
      "free": 8388608,
      "usage_percent": 50.0
    }
  }
}
```
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `message` (string): 结果描述
- **响应示例**：
```json
{
  "status": "success",
  "message": "Resource usage saved successfully"
}
```

### 4. 获取板卡列表
- **GET** `/api/boards`
- **响应字段说明**：
  - `status` (string): "success"
  - `boards` (array): 板卡对象数组
- **板卡对象示例**：
```json
{
  "board_id": "board-xxxx",
  "hostname": "node-1",
  "ip_address": "192.168.1.100",
  "os_info": "Ubuntu 22.04",
  "created_at": 1710000000,
  "updated_at": 1710000000,
  "status": "online"
}
```
- **响应示例**：
```json
{
  "status": "success",
  "boards": [ /* 板卡对象数组 */ ]
}
```

### 5. 获取板卡详情
- **GET** `/api/boards/:board_id`
- **响应字段说明**：
  - `status` (string): "success" 或 "error"
  - `board` (object): 板卡对象
- **响应示例**：
```json
{
  "status": "success",
  "board": {
    "board_id": "board-xxxx",
    "hostname": "node-1",
    "ip_address": "192.168.1.100",
    "os_info": "Ubuntu 22.04",
    "created_at": 1710000000,
    "updated_at": 1710000000,
    "status": "online"
  }
}
```

### 6. 获取板卡资源历史
- **GET** `/api/boards/:board_id/resources?limit=100`
- **响应字段说明**：
  - `status` (string): "success"
  - `history` (object):
    - `cpu_metrics` (array): CPU指标数组
    - `memory_metrics` (array): 内存指标数组
- **CPU指标对象示例**：
```json
{
  "timestamp": 1710000000,
  "usage_percent": 23.5,
  "load_avg_1m": 0.5,
  "load_avg_5m": 0.4,
  "load_avg_15m": 0.3,
  "core_count": 8
}
```
- **内存指标对象示例**：
```json
{
  "timestamp": 1710000000,
  "total": 16777216,
  "used": 8388608,
  "free": 8388608,
  "usage_percent": 50.0
}
```
- **响应示例**：
```json
{
  "status": "success",
  "history": {
    "cpu_metrics": [ /* CPU指标对象数组 */ ],
    "memory_metrics": [ /* 内存指标对象数组 */ ]
  }
}
```

### 7. 获取板卡资源明细
- **GET** `/api/boards/:board_id/resources/:resource_type?limit=100`
- **响应字段说明**：
  - `status` (string): "success"
  - `cpu_metrics` (array): 当resource_type为cpu时，返回CPU指标数组
  - `memory_metrics` (array): 当resource_type为memory时，返回内存指标数组
- **响应示例（CPU）**：
```json
{
  "status": "success",
  "cpu_metrics": [
    {
      "timestamp": 1710000000,
      "usage_percent": 23.5,
      "load_avg_1m": 0.5,
      "load_avg_5m": 0.4,
      "load_avg_15m": 0.3,
      "core_count": 8
    }
  ]
}
```
- **响应示例（内存）**：
```json
{
  "status": "success",
  "memory_metrics": [
    {
      "timestamp": 1710000000,
      "total": 16777216,
      "used": 8388608,
      "free": 8388608,
      "usage_percent": 50.0
    }
  ]
}
``` 