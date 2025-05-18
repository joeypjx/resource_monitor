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

## API接口

### 资源监控API

#### 获取所有Agent列表

```
GET /api/agents
```

#### 获取指定Agent的资源数据

```
GET /api/agents/{agent_id}/{resource_type}?limit={limit}
```

参数说明：
- `agent_id`：Agent ID
- `resource_type`：资源类型，可选值：cpu, memory, disk, network, docker
- `limit`：返回记录数量限制，默认为100

### 业务部署与管理API（Manager端）

#### 部署业务

```
POST /api/businesses
```

请求体示例：
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
        "require_gpu": false
      }
    },
    {
      "component_id": "component-2",
      "component_name": "my-binary",
      "type": "binary",
      "binary_path": "/opt/bin/my-binary",
      "binary_url": "http://example.com/bin/my-binary",
      "resource_requirements": {
        "cpu_cores": 1,
        "memory_mb": 256
      },
      "environment_variables": {
        "MODE": "test"
      },
      "config_files": [],
      "affinity": {}
    }
  ]
}
```

#### 停止业务

```
POST /api/businesses/{business_id}/stop
```

#### 重启业务

```
POST /api/businesses/{business_id}/restart
```

#### 更新业务

```
PUT /api/businesses/{business_id}
```

请求体格式同部署业务。

#### 获取业务列表

```
GET /api/businesses
```

#### 获取业务详情

```
GET /api/businesses/{business_id}
```

#### 获取业务组件状态

```
GET /api/businesses/{business_id}/components
```

#### 组件状态上报

```
POST /api/report/component
```
请求体示例：
```json
{
  "component_id": "component-1",
  "business_id": "unique-business-id",
  "status": "running",
  "container_id": "xxxxxx"
}
```

### Agent本地API（组件部署/停止）

Agent本地HTTP服务（默认8081端口）支持组件部署与停止：

- 部署组件：
  - `POST /api/deploy`，请求体为JSON，包含component_id、business_id、component_name、type等字段
- 停止组件：
  - `POST /api/stop`，请求体为JSON，包含component_id、business_id等字段

> 注意：Agent上报组件状态时，暂不包含node_id字段（即agent_id），如需扩展可在后续版本中支持。

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

## 许可证

MIT
