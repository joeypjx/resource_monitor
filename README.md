# 资源监控系统

这是一个基于C++14开发的分布式资源监控系统，由Agent和Manager两部分组成。Agent运行在工作节点上收集资源信息，Manager作为中心管理端接收和存储数据。

## 系统架构

系统由以下两个主要组件构成：

- **Agent**：部署在各个工作节点上，负责采集本地资源信息并上报给Manager
- **Manager**：作为中心管理节点，接收来自各个Agent的数据，并存储到SQLite数据库中

![系统架构](docs/architecture.png)

## 功能特性

- 资源监控：CPU、内存、磁盘、网络和Docker容器
- 自动注册：Agent自动向Manager注册
- 定时上报：Agent每5秒采集并上报资源数据
- 数据存储：Manager将数据存储到SQLite数据库
- API接口：提供查询接口，为未来的Web UI做准备

## 编译与安装

### 依赖项

- C++14兼容的编译器
- CMake 3.10+
- SQLite3
- CURL
- UUID库

在Ubuntu系统上，可以使用以下命令安装依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev libcurl4-openssl-dev uuid-dev
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

Manager提供以下HTTP API接口：

### 获取所有Agent列表

```
GET /api/agents
```

### 获取指定Agent的资源数据

```
GET /api/agents/{agent_id}/{resource_type}?limit={limit}
```

参数说明：
- `agent_id`：Agent ID
- `resource_type`：资源类型，可选值：cpu, memory, disk, network, docker
- `limit`：返回记录数量限制，默认为100

## 测试

使用提供的测试脚本进行功能测试：

```bash
./test.sh
```

测试脚本将启动Manager和Agent，并验证数据采集、上报和API接口功能。

## 未来扩展

- Web UI：提供资源监控的可视化界面
- 数据聚合：支持数据的时间维度聚合，减少存储空间占用
- 告警机制：当资源使用超过阈值时，发送告警通知
- 安全性：添加认证和加密机制，保护通信安全
- 配置管理：支持动态调整资源采集周期和采集项

## 许可证

MIT
