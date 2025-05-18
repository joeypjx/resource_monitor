# 资源监控与业务部署系统

这是一个基于C++14开发的分布式系统，由Agent和Manager两部分组成。系统不仅能够监控节点资源信息，还能够进行业务部署和管理。

## 系统架构

系统由以下两个主要组件构成：

- **Agent**：部署在各个工作节点上，负责采集本地资源信息、部署和管理业务组件
- **Manager**：作为中心管理节点，接收来自各个Agent的数据，进行资源调度，并存储到SQLite数据库中

![系统架构](docs/architecture.png)

## 功能特性

### 资源监控功能
- 资源监控：CPU、内存、磁盘、网络和Docker容器
- 自动注册：Agent自动向Manager注册
- 定时上报：Agent每5秒采集并上报资源数据
- 数据存储：Manager将数据存储到SQLite数据库

### 业务部署功能
- 业务部署：支持通过JSON配置部署多组件业务（支持Docker和二进制类型组件）
- 资源调度：根据节点资源情况和亲和性进行智能调度
- Docker支持：支持Docker容器的部署和管理
- 二进制支持：支持二进制可执行文件的分发与运行
- 状态监控：收集业务组件的运行状态
- 生命周期管理：支持业务的停止、重启和更新

## 编译与安装

### 依赖项

- C++14兼容的编译器
- CMake 3.10+
- SQLite3
- CURL
- UUID库
- Docker（用于业务部署功能）

在Ubuntu系统上，可以使用以下命令安装依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev libcurl4-openssl-dev uuid-dev docker.io
```

### 编译

使用提供的构建脚本进行编译：

```bash
./build.sh
```

编译成功后，将在`build`目录下生成`agent`和`manager`两个可执行文件。

## 使用方法

### 启动Manager

```bash
cd build
./manager [--port <端口>] [--db-path <数据库路径>]
```

参数说明：
- `--port`：HTTP服务器端口，默认为8080
- `--db-path`：数据库文件路径，默认为resource_monitor.db

### 启动Agent

```bash
cd build
./agent [--manager-url <Manager URL>] [--hostname <主机名>] [--interval <采集间隔>]
```

参数说明：
- `--manager-url`：Manager的URL地址，默认为http://localhost:8080
- `--hostname`：主机名，默认自动获取
- `--interval`：资源采集间隔（秒），默认为5

---

# API接口文档（前端开发专用）

## 1. 节点与资源监控

### 获取所有Agent列表
- **GET** `/api/agents`
- **返回示例**
  ```json
  {
    "status": "success",
    "agents": [
      {
        "agent_id": "node-xxxx",
        "hostname": "host1",
        "ip": "192.168.1.10",
        "last_heartbeat": "2024-05-18 10:00:00"
      }
    ]
  }
  ```

### 获取指定Agent的资源历史
- **GET** `/api/agents/{agent_id}/resources?limit=100`
- **参数**
  - `agent_id`：节点ID
  - `limit`：返回条数（可选，默认100）
- **返回示例**
  ```json
  {
    "status": "success",
    "history": [
      {
        "timestamp": "2024-05-18 10:00:00",
        "cpu": 0.12,
        "memory": 512,
        "disk": 10240,
        "network": 1234
      }
    ]
  }
  ```

### 获取指定Agent的某类资源
- **GET** `/api/agents/{agent_id}/resources/{resource_type}?limit=100`
- **resource_type**：`cpu`/`memory`/`disk`/`network`/`docker`
- **返回示例**
  ```json
  {
    "status": "success",
    "metrics": [
      {
        "timestamp": "2024-05-18 10:00:00",
        "value": 0.12
      }
    ]
  }
  ```

---

## 2. 业务部署与管理

### 部署业务
- **POST** `/api/businesses`
- **请求体**
  ```json
  {
    "business_name": "example-business",
    "components": [
      {
        "component_id": "component-1",
        "component_name": "web-server",
        "type": "docker",
        "image_url": "https://example.com/images/web-server.tar",
        "image_name": "web-server:latest",
        "resource_requirements": {"cpu_cores": 2, "memory_mb": 1024, "gpu": false},
        "environment_variables": {"PORT": "8080"},
        "config_files": [],
        "affinity": {"require_gpu": false}
      },
      {
        "component_id": "component-2",
        "component_name": "my-binary",
        "type": "binary",
        "binary_path": "/opt/bin/my-binary",
        "binary_url": "http://example.com/bin/my-binary",
        "resource_requirements": {"cpu_cores": 1, "memory_mb": 256},
        "environment_variables": {"MODE": "test"},
        "config_files": [],
        "affinity": {}
      }
    ]
  }
  ```
- **返回示例**
  ```json
  {
    "status": "success",
    "business_id": "business-xxxx",
    "message": "Business deployed"
  }
  ```

### 停止业务
- **POST** `/api/businesses/{business_id}/stop`
- **返回示例**
  ```json
  {"status": "success", "message": "Business stopped"}
  ```

### 重启业务
- **POST** `/api/businesses/{business_id}/restart`
- **返回示例**
  ```json
  {"status": "success", "message": "Business restarted"}
  ```

### 更新业务
- **PUT** `/api/businesses/{business_id}`
- **请求体**：同部署业务
- **返回示例**
  ```json
  {"status": "success", "message": "Business updated"}
  ```

### 获取业务列表
- **GET** `/api/businesses`
- **返回示例**
  ```json
  {
    "status": "success",
    "businesses": [
      {
        "business_id": "business-xxxx",
        "business_name": "example-business",
        "status": "running"
      }
    ]
  }
  ```

### 获取业务详情
- **GET** `/api/businesses/{business_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "business": {
      "business_id": "business-xxxx",
      "business_name": "example-business",
      "status": "running",
      "components": [ ... ]
    }
  }
  ```

### 获取业务组件状态
- **GET** `/api/businesses/{business_id}/components`
- **返回示例**
  ```json
  {
    "status": "success",
    "components": [
      {
        "component_id": "component-1",
        "status": "running",
        "type": "docker",
        "node_id": "node-xxxx"
      }
    ]
  }
  ```

### 组件状态上报
- **POST** `/api/report/component`
- **请求体**
  ```json
  {
    "component_id": "component-1",
    "business_id": "business-xxxx",
    "status": "running",
    "container_id": "xxxxxx"
  }
  ```
- **返回示例**
  ```json
  {"status": "success"}
  ```

---

## 3. 组件模板管理

### 创建组件模板
- **POST** `/api/templates/components`
- **请求体**
  ```json
  {
    "template_name": "nginx-docker-template",
    "description": "Nginx docker组件模板",
    "type": "docker",
    "config": {
      "image_url": "nginx:latest",
      "image_name": "nginx:latest",
      "resource_requirements": {"cpu_cores": 1, "memory_mb": 128},
      "environment_variables": {"PORT": "80"},
      "config_files": [],
      "affinity": {}
    }
  }
  ```
- **返回示例**
  ```json
  {"status": "success", "component_template_id": "ct-xxxx"}
  ```

### 获取组件模板列表
- **GET** `/api/templates/components`
- **返回示例**
  ```json
  {
    "status": "success",
    "templates": [
      {
        "component_template_id": "ct-xxxx",
        "template_name": "nginx-docker-template",
        "type": "docker"
      }
    ]
  }
  ```

### 获取组件模板详情
- **GET** `/api/templates/components/{template_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "template": {
      "component_template_id": "ct-xxxx",
      "template_name": "nginx-docker-template",
      "type": "docker",
      "description": "Nginx docker组件模板",
      "config": { ... }
    }
  }
  ```

### 更新组件模板
- **PUT** `/api/templates/components/{template_id}`
- **请求体**：同创建
- **返回示例**
  ```json
  {"status": "success"}
  ```

### 删除组件模板
- **DELETE** `/api/templates/components/{template_id}`
- **返回示例**
  ```json
  {"status": "success"}
  ```

---

## 4. 业务模板管理

### 创建业务模板
- **POST** `/api/templates/businesses`
- **请求体**
  ```json
  {
    "template_name": "web-app-template",
    "description": "Web应用业务模板",
    "components": [
      {
        "component_template_id": "ct-xxxx",
        "component_id": "component-1",
        "component_name": "nginx-web"
      },
      {
        "component_template_id": "ct-yyyy",
        "component_id": "component-2",
        "component_name": "my-binary"
      }
    ]
  }
  ```
- **返回示例**
  ```json
  {"status": "success", "business_template_id": "bt-xxxx"}
  ```

### 获取业务模板列表
- **GET** `/api/templates/businesses`
- **返回示例**
  ```json
  {
    "status": "success",
    "templates": [
      {
        "business_template_id": "bt-xxxx",
        "template_name": "web-app-template"
      }
    ]
  }
  ```

### 获取业务模板详情
- **GET** `/api/templates/businesses/{template_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "template": {
      "business_template_id": "bt-xxxx",
      "template_name": "web-app-template",
      "description": "Web应用业务模板",
      "components": [
        {
          "component_template_id": "ct-xxxx",
          "component_id": "component-1",
          "component_name": "nginx-web",
          "template_details": { ... }
        }
      ]
    }
  }
  ```

### 更新业务模板
- **PUT** `/api/templates/businesses/{template_id}`
- **请求体**：同创建
- **返回示例**
  ```json
  {"status": "success"}
  ```

### 删除业务模板
- **DELETE** `/api/templates/businesses/{template_id}`
- **返回示例**
  ```json
  {"status": "success"}
  ```

### 获取业务模板部署配置（转为实际部署格式）
- **GET** `/api/templates/businesses/{template_id}/as-business`
- **返回示例**
  ```json
  {
    "status": "success",
    "business": {
      "business_name": "web-app-template",
      "components": [
        {
          "component_id": "component-1",
          "component_name": "nginx-web",
          "type": "docker",
          "image_url": "nginx:1.21",
          "image_name": "nginx:1.21",
          "resource_requirements": {"cpu_cores": 2, "memory_mb": 256},
          "environment_variables": {"PORT": "8080"},
          "config_files": [],
          "affinity": {}
        }
      ]
    }
  }
  ```

---

## 5. Agent本地API（组件部署/停止）

### 部署组件
- **POST** `/api/deploy`
- **请求体**
  ```json
  {
    "component_id": "component-1",
    "business_id": "business-xxxx",
    "component_name": "web-server",
    "type": "docker"
  }
  ```
- **返回示例**
  ```json
  {"status": "success"}
  ```

### 停止组件
- **POST** `/api/stop`
- **请求体**
  ```json
  {
    "component_id": "component-1",
    "business_id": "business-xxxx"
  }
  ```
- **返回示例**
  ```json
  {"status": "success"}
  ```

---

## 字段说明

- `component_id`：组件唯一标识
- `component_name`：组件名称
- `type`：组件类型（docker/binary）
- `image_url`/`image_name`：docker镜像相关
- `binary_url`/`binary_path`：二进制组件相关
- `resource_requirements`：资源需求（cpu_cores、memory_mb、gpu）
- `environment_variables`：环境变量
- `config_files`：配置文件数组
- `affinity`：调度亲和性约束
- `status`：状态（success/error/running/stopped等）

---

如需补充更多接口或字段说明，请随时告知！

## 测试

### 资源监控测试

使用提供的测试脚本进行功能测试：

```bash
./test.sh
```

### 业务部署测试

使用提供的业务部署测试脚本：

```bash
./deployment_test.sh
```

## 系统架构详情

系统架构详细设计请参考：
- [资源监控架构](docs/system_architecture.md)
- [业务部署架构](docs/deployment/deployment_architecture.md)

## 后续改进

- 接口改成异步
- 接口/agent全部改成/node，数据库字段也改成node
- node信息完善
- 完善调度机制
- 不常用接口检查和删除
- 前端开发

## 许可证

MIT
