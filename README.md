# 资源监控与业务部署系统

这是一个基于C++14开发的分布式系统，由Agent和Manager两部分组成。系统不仅能够监控计算板卡(Board)资源信息，还能够进行业务部署和管理，支持机箱级别的组织管理和详细的硬件配置记录。

## 系统架构

系统由以下两个主要组件构成：

- **Agent**：部署在各个计算板卡(Board)上，负责采集本地资源信息、部署和管理业务组件
- **Manager**：作为中心管理节点，接收来自各个Agent的数据，进行资源调度，并存储到SQLite数据库中

![系统架构](docs/architecture.png)

## 功能特性

### 资源监控功能
- **资源监控**：CPU、内存、磁盘、网络、Docker容器和GPU
- **自动注册**：Agent自动向Manager注册，包含完整硬件配置信息
- **定时上报**：Agent每5秒采集并上报资源数据
- **数据存储**：Manager将数据存储到SQLite数据库
- **机箱管理**：支持机箱级别的组织管理，可将多个Board归属到同一机箱
- **硬件管理**：详细记录和管理CPU、GPU等硬件配置信息

### 业务部署功能
- **业务部署**：支持通过JSON配置部署多组件业务（支持Docker和二进制类型组件）
- **资源调度**：根据节点资源情况和亲和性进行智能调度
- **Docker支持**：支持Docker容器的部署和管理
- **二进制支持**：支持二进制可执行文件的分发与运行
- **状态监控**：收集业务组件的运行状态
- **生命周期管理**：支持业务的停止、重启和更新

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

## 1. Board与资源监控

### 获取所有Board列表
- **GET** `/api/boards`
- **返回示例**
  ```json
  {
    "status": "success",
    "boards": [
      {
        "board_id": "board-xxxx",
        "hostname": "host1",
        "ip_address": "192.168.1.10", 
        "os_info": "Ubuntu 20.04",
        "chassis_id": "chassis-001",
        "status": "online",
        "created_at": 1710000000,
        "updated_at": 1710000005,
        "cpu_list": [
          {
            "processor_id": 0,
            "model_name": "Intel Core i7-8700K",
            "vendor": "Intel",
            "frequency_mhz": 3700.0,
            "cache_size": "12288 KB",
            "cores": 6
          }
        ],
        "gpu_list": [
          {
            "index": 0,
            "name": "NVIDIA GeForce RTX 3080",
            "memory_total_mb": 10240,
            "temperature_c": 45,
            "utilization_percent": 0,
            "type": "discrete"
          }
        ]
      }
    ]
  }
  ```

### 获取指定Board信息
- **GET** `/api/boards/{board_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "board": {
      "board_id": "board-xxxx",
      "hostname": "host1",
      "ip_address": "192.168.1.10",
      "os_info": "Ubuntu 20.04",
      "chassis_id": "chassis-001",
      "status": "online",
      "cpu_list": [...],
      "gpu_list": [...]
    }
  }
  ```

### 获取指定Board的资源历史
- **GET** `/api/boards/{board_id}/resources?limit=100`
- **参数**
  - `board_id`：Board ID
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

### 获取指定Board的某类资源
- **GET** `/api/boards/{board_id}/resources/{resource_type}?limit=100`
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

## 2. 机箱管理

### 创建机箱
- **POST** `/api/chassis`
- **请求体**
  ```json
  {
    "chassis_id": "chassis-001",
    "chassis_name": "主机箱1号",
    "description": "数据中心A区主机箱",
    "location": "A区-1排-3号"
  }
  ```
- **返回示例**
  ```json
  {
    "status": "success",
    "message": "Chassis created successfully"
  }
  ```

### 获取机箱列表
- **GET** `/api/chassis`
- **返回示例**
  ```json
  {
    "status": "success",
    "chassis": [
      {
        "chassis_id": "chassis-001",
        "chassis_name": "主机箱1号",
        "description": "数据中心A区主机箱",
        "location": "A区-1排-3号",
        "created_at": 1710000000,
        "updated_at": 1710000000
      }
    ]
  }
  ```

### 获取指定机箱详情
- **GET** `/api/chassis/{chassis_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "chassis": {
      "chassis_id": "chassis-001",
      "chassis_name": "主机箱1号",
      "description": "数据中心A区主机箱",
      "location": "A区-1排-3号",
      "created_at": 1710000000,
      "updated_at": 1710000000
    }
  }
  ```

### 更新机箱信息
- **PUT** `/api/chassis/{chassis_id}`
- **请求体**：同创建机箱
- **返回示例**
  ```json
  {
    "status": "success",
    "message": "Chassis updated successfully"
  }
  ```

### 删除机箱
- **DELETE** `/api/chassis/{chassis_id}`
- **返回示例**
  ```json
  {
    "status": "success",
    "message": "Chassis deleted successfully"
  }
  ```

### 获取机箱下的Board列表
- **GET** `/api/chassis/{chassis_id}/boards`
- **返回示例**
  ```json
  {
    "status": "success",
    "boards": [
      {
        "board_id": "board-xxxx",
        "hostname": "host1",
        "chassis_id": "chassis-001",
        "status": "online",
        "cpu_list": [...],
        "gpu_list": [...]
      }
    ]
  }
  ```

---

## 3. 硬件管理

### 获取指定Board的CPU列表
- **GET** `/api/boards/{board_id}/cpus`
- **返回示例**
  ```json
  {
    "status": "success",
    "cpus": [
      {
        "processor_id": 0,
        "model_name": "Intel Core i7-8700K",
        "vendor": "Intel",
        "frequency_mhz": 3700.0,
        "cache_size": "12288 KB",
        "cores": 6,
        "created_at": 1710000000,
        "updated_at": 1710000000
      }
    ]
  }
  ```

### 获取指定Board的GPU列表
- **GET** `/api/boards/{board_id}/gpus`
- **返回示例**
  ```json
  {
    "status": "success",
    "gpus": [
      {
        "index": 0,
        "name": "NVIDIA GeForce RTX 3080",
        "memory_total_mb": 10240,
        "temperature_c": 45,
        "utilization_percent": 0,
        "type": "discrete",
        "created_at": 1710000000,
        "updated_at": 1710000000
      }
    ]
  }
  ```

### 获取所有CPU列表
- **GET** `/api/cpus`
- **返回示例**
  ```json
  {
    "status": "success",
    "cpus": [
      {
        "board_id": "board-xxxx",
        "processor_id": 0,
        "model_name": "Intel Core i7-8700K",
        "vendor": "Intel",
        "frequency_mhz": 3700.0,
        "cache_size": "12288 KB",
        "cores": 6,
        "hostname": "host1",
        "chassis_id": "chassis-001",
        "created_at": 1710000000,
        "updated_at": 1710000000
      }
    ]
  }
  ```

### 获取所有GPU列表
- **GET** `/api/gpus`
- **返回示例**
  ```json
  {
    "status": "success",
    "gpus": [
      {
        "board_id": "board-xxxx",
        "gpu_index": 0,
        "name": "NVIDIA GeForce RTX 3080",
        "memory_total_mb": 10240,
        "temperature_c": 45,
        "utilization_percent": 0,
        "type": "discrete",
        "hostname": "host1",
        "chassis_id": "chassis-001",
        "created_at": 1710000000,
        "updated_at": 1710000000
      }
    ]
  }
  ```

---

## 4. 业务部署与管理

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
        "board_id": "board-xxxx"
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

## 5. 组件模板管理

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

## 6. 业务模板管理

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

## 7. Agent本地API（组件部署/停止）

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

## 8. 基础协议接口

### Board注册接口
- **POST** `/api/register`
- **请求体**
  ```json
  {
    "board_id": "board-xxxx",
    "hostname": "host1",
    "ip_address": "192.168.1.10",
    "os_info": "Ubuntu 20.04",
    "chassis_id": "chassis-001",
    "cpu_list": [
      {
        "processor_id": 0,
        "model_name": "Intel Core i7-8700K",
        "vendor": "Intel",
        "frequency_mhz": 3700.0,
        "cache_size": "12288 KB",
        "cores": 6
      }
    ],
    "gpu_list": [
      {
        "index": 0,
        "name": "NVIDIA GeForce RTX 3080",
        "memory_total_mb": 10240,
        "temperature_c": 45,
        "utilization_percent": 0,
        "type": "discrete"
      }
    ]
  }
  ```
- **返回示例**
  ```json
  {
    "status": "success",
    "message": "Board registered successfully",
    "board_id": "board-xxxx"
  }
  ```

### 资源上报数据格式（Agent -> Manager）
- **POST** `/api/report`
- **请求体**
  ```json
  {
    "board_id": "board-xxxx",
    "timestamp": 1710000000,
    "resource": {
      "cpu": {
        "usage_percent": 12.5,
        "load_avg_1m": 0.5,
        "load_avg_5m": 0.4,
        "load_avg_15m": 0.3,
        "core_count": 4
      },
      "memory": {
        "total": 8192,
        "used": 4096,
        "free": 4096,
        "usage_percent": 50.0
      },
      "disk": [
        {
          "device": "/dev/sda1",
          "mount_point": "/",
          "total": 100000,
          "used": 50000,
          "free": 50000,
          "usage_percent": 50.0
        }
      ],
      "network": [
        {
          "interface": "eth0",
          "rx_bytes": 123456,
          "tx_bytes": 654321,
          "rx_packets": 1000,
          "tx_packets": 900,
          "rx_errors": 0,
          "tx_errors": 0
        }
      ],
      "docker": {
        "container_count": 2,
        "running_count": 2,
        "paused_count": 0,
        "stopped_count": 0,
        "containers": [
          {
            "id": "abc123",
            "name": "nginx",
            "image": "nginx:latest",
            "status": "running",
            "cpu_percent": 1.2,
            "memory_usage": 128
          }
        ]
      }
    }
  }
  ```
- **说明**：所有资源数据均在 resource 字段下，字段类型和内容如上所示。

---

## 字段说明

### 核心概念
- `board_id`：计算板卡唯一标识
- `chassis_id`：机箱唯一标识
- `component_id`：组件唯一标识
- `business_id`：业务唯一标识

### 硬件相关
- `processor_id`：CPU处理器ID
- `gpu_index`：GPU索引
- `model_name`：硬件型号名称
- `vendor`：硬件厂商
- `frequency_mhz`：CPU频率（MHz）
- `cache_size`：缓存大小
- `cores`：CPU核心数
- `memory_total_mb`：GPU显存总量（MB）
- `temperature_c`：温度（摄氏度）
- `utilization_percent`：GPU利用率（百分比）

### 业务相关
- `component_name`：组件名称
- `type`：组件类型（docker/binary）
- `image_url`/`image_name`：docker镜像相关
- `binary_url`/`binary_path`：二进制组件相关
- `resource_requirements`：资源需求（cpu_cores、memory_mb、gpu）
- `environment_variables`：环境变量
- `config_files`：配置文件数组
- `affinity`：调度亲和性约束
- `status`：状态（success/error/running/stopped/online/offline等）

---

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
- 完善调度机制
- 不常用接口检查和删除
- 前端开发
- 增加更多硬件类型支持
- 完善机箱级别的管理功能
- 添加集群监控和告警功能

## 许可证

MIT
