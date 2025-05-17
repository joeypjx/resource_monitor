# 业务部署功能架构设计

## 1. 概述

在原有资源监控系统的基础上，扩展业务部署功能，使系统能够接收业务部署请求，根据节点资源情况进行智能调度，并收集业务组件的运行状态。

## 2. 系统架构扩展

扩展后的系统架构如下：

```
+----------------+    +----------------+    +----------------+
|    Agent 1     |    |    Agent 2     |    |    Agent N     |
| (工作节点1)     |    | (工作节点2)     |    | (工作节点N)     |
+-------+--------+    +-------+--------+    +-------+--------+
        |                     |                     |
        |  HTTP协议           |                     |
        +---------------------+---------------------+
                              |
                     +--------v---------+
                     |     Manager      |
                     | (中心管理节点)     |
                     +--------+---------+
                              |
                     +--------v---------+
                     |  SQLite数据库     |
                     | (资源和业务数据)   |
                     +------------------+
```

### 2.1 Manager扩展

Manager新增以下功能模块：

1. **业务部署API**：接收业务部署请求
2. **资源调度器**：根据节点资源情况进行业务组件调度
3. **业务状态管理**：管理业务生命周期和状态
4. **业务数据存储**：存储业务和组件信息

### 2.2 Agent扩展

Agent新增以下功能模块：

1. **组件管理器**：负责启动、停止和更新业务组件
2. **Docker接口**：与Docker交互，管理容器
3. **组件状态收集**：收集业务组件的运行状态
4. **组件状态上报**：将组件状态上报给Manager

## 3. 数据结构设计

### 3.1 业务部署请求格式

```json
{
  "business_id": "unique-business-id",
  "business_name": "example-business",
  "components": [
    {
      "component_id": "component-1",
      "component_name": "web-server",
      "type": "docker",
      "image_url": "https://example.com/images/web-server.tar",
      "image_name": "web-server:latest",
      "resource_requirements": {
        "cpu_cores": 2,
        "memory_mb": 1024,
        "gpu": false
      },
      "environment_variables": {
        "PORT": "8080",
        "DB_HOST": "localhost"
      },
      "config_files": [
        {
          "path": "/etc/web-server/config.json",
          "content": "{\"key\": \"value\"}"
        }
      ],
      "affinity": {
        "require_gpu": false,
        "node_labels": {
          "zone": "us-east"
        }
      }
    }
  ]
}
```

### 3.2 业务状态数据结构

```json
{
  "business_id": "unique-business-id",
  "business_name": "example-business",
  "status": "running",
  "created_at": "2025-05-17T07:30:00Z",
  "updated_at": "2025-05-17T07:35:00Z",
  "components": [
    {
      "component_id": "component-1",
      "component_name": "web-server",
      "node_id": "node-1",
      "status": "running",
      "container_id": "abc123def456",
      "resource_usage": {
        "cpu_percent": 15.5,
        "memory_mb": 850,
        "gpu_percent": 0
      },
      "started_at": "2025-05-17T07:31:00Z",
      "updated_at": "2025-05-17T07:35:00Z"
    }
  ]
}
```

## 4. 数据库扩展设计

### 4.1 businesses表

存储业务信息

```sql
CREATE TABLE businesses (
    business_id TEXT PRIMARY KEY,
    business_name TEXT NOT NULL,
    status TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL,
    updated_at TIMESTAMP NOT NULL
);
```

### 4.2 business_components表

存储业务组件信息

```sql
CREATE TABLE business_components (
    component_id TEXT PRIMARY KEY,
    business_id TEXT NOT NULL,
    component_name TEXT NOT NULL,
    type TEXT NOT NULL,
    image_url TEXT,
    image_name TEXT,
    resource_requirements TEXT NOT NULL,
    environment_variables TEXT,
    config_files TEXT,
    affinity TEXT,
    node_id TEXT,
    container_id TEXT,
    status TEXT NOT NULL,
    started_at TIMESTAMP,
    updated_at TIMESTAMP,
    FOREIGN KEY (business_id) REFERENCES businesses(business_id),
    FOREIGN KEY (node_id) REFERENCES agents(agent_id)
);
```

### 4.3 component_metrics表

存储组件资源使用指标

```sql
CREATE TABLE component_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    component_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    cpu_percent REAL NOT NULL,
    memory_mb INTEGER NOT NULL,
    gpu_percent REAL,
    FOREIGN KEY (component_id) REFERENCES business_components(component_id)
);
```

## 5. API接口设计

### 5.1 业务部署接口

- **URL**: `/api/businesses`
- **方法**: POST
- **请求体**: 业务部署请求JSON
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息",
    "business_id": "已部署的业务ID"
  }
  ```

### 5.2 业务停止接口

- **URL**: `/api/businesses/{business_id}/stop`
- **方法**: POST
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息"
  }
  ```

### 5.3 业务重启接口

- **URL**: `/api/businesses/{business_id}/restart`
- **方法**: POST
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息"
  }
  ```

### 5.4 业务更新接口

- **URL**: `/api/businesses/{business_id}`
- **方法**: PUT
- **请求体**: 更新后的业务部署请求JSON
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息"
  }
  ```

### 5.5 获取业务列表接口

- **URL**: `/api/businesses`
- **方法**: GET
- **响应**: 业务列表JSON数组

### 5.6 获取业务详情接口

- **URL**: `/api/businesses/{business_id}`
- **方法**: GET
- **响应**: 业务详情JSON

### 5.7 获取业务组件状态接口

- **URL**: `/api/businesses/{business_id}/components`
- **方法**: GET
- **响应**: 业务组件状态JSON数组

## 6. Agent-Manager通信协议扩展

### 6.1 组件部署请求

- **URL**: `/api/deploy`
- **方法**: POST
- **请求体**:
  ```json
  {
    "component_id": "component-1",
    "business_id": "unique-business-id",
    "component_name": "web-server",
    "type": "docker",
    "image_url": "https://example.com/images/web-server.tar",
    "image_name": "web-server:latest",
    "resource_requirements": {
      "cpu_cores": 2,
      "memory_mb": 1024,
      "gpu": false
    },
    "environment_variables": {
      "PORT": "8080",
      "DB_HOST": "localhost"
    },
    "config_files": [
      {
        "path": "/etc/web-server/config.json",
        "content": "{\"key\": \"value\"}"
      }
    ]
  }
  ```
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息",
    "container_id": "容器ID（如果成功）"
  }
  ```

### 6.2 组件停止请求

- **URL**: `/api/stop`
- **方法**: POST
- **请求体**:
  ```json
  {
    "component_id": "component-1",
    "business_id": "unique-business-id",
    "container_id": "abc123def456"
  }
  ```
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息"
  }
  ```

### 6.3 组件状态上报

- **URL**: `/api/report/component`
- **方法**: POST
- **请求体**:
  ```json
  {
    "component_id": "component-1",
    "business_id": "unique-business-id",
    "node_id": "node-1",
    "status": "running",
    "container_id": "abc123def456",
    "resource_usage": {
      "cpu_percent": 15.5,
      "memory_mb": 850,
      "gpu_percent": 0
    },
    "timestamp": 1621234567
  }
  ```
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息"
  }
  ```

## 7. 调度算法设计

调度算法将考虑以下因素：

1. **节点资源可用性**：CPU、内存、GPU等资源的可用量
2. **节点亲和性**：组件对特定节点类型的需求（如GPU）
3. **负载均衡**：尽量使各节点的资源利用率均衡

调度流程：

1. 过滤不满足资源需求的节点
2. 过滤不满足亲和性要求的节点
3. 根据负载均衡策略，选择最优节点
4. 如果没有合适节点，返回调度失败

## 8. 业务生命周期管理

业务状态包括：

- **pending**：业务已提交，等待调度
- **deploying**：业务组件正在部署
- **running**：业务正常运行
- **failed**：业务部署或运行失败
- **stopping**：业务正在停止
- **stopped**：业务已停止
- **updating**：业务正在更新

组件状态包括：

- **pending**：组件等待部署
- **downloading**：正在下载组件镜像
- **starting**：组件正在启动
- **running**：组件正常运行
- **failed**：组件启动或运行失败
- **stopping**：组件正在停止
- **stopped**：组件已停止

## 9. 数据流

1. **业务部署流程**：
   - Manager接收业务部署请求
   - Manager进行资源调度，为每个组件选择合适节点
   - Manager向选中的Agent发送组件部署请求
   - Agent下载镜像并启动容器
   - Agent将组件状态上报给Manager
   - Manager更新业务和组件状态

2. **状态收集流程**：
   - Agent定期收集组件运行状态
   - Agent将组件状态上报给Manager
   - Manager更新数据库中的组件状态

3. **业务停止流程**：
   - Manager接收业务停止请求
   - Manager向相关Agent发送组件停止请求
   - Agent停止组件并上报状态
   - Manager更新业务和组件状态

4. **业务更新流程**：
   - Manager接收业务更新请求
   - Manager计算需要停止、更新和新增的组件
   - Manager按需向Agent发送停止和部署请求
   - Agent执行相应操作并上报状态
   - Manager更新业务和组件状态

## 10. 技术选型

继续使用原有技术栈：

- C++14标准
- Linux平台
- HTTP通信（cpp-httplib）
- JSON处理（nlohmann/json）
- SQLite数据库（SQLiteCpp）
- Docker API（通过HTTP与Docker守护进程通信）

## 11. 未来扩展

虽然当前不实现，但系统设计考虑了以下未来扩展点：

1. **组件依赖关系和启动顺序**
2. **业务组件的扩缩容**
3. **业务组件的日志收集**
4. **业务的健康检查和自动恢复**
5. **优雅关闭（graceful shutdown）**
