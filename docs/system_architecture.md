# 资源监控系统架构设计

## 1. 整体架构

资源监控系统由两个主要组件构成：Agent和Manager。

- **Agent**：部署在各个工作节点上，负责采集本地资源信息并上报给Manager
- **Manager**：作为中心管理节点，接收来自各个Agent的数据，并存储到SQLite数据库中

系统架构图如下：

```
+----------------+    +----------------+    +----------------+
|    Agent 1     |    |    Agent 2     |    |    Agent N     |
| (工作节点1)     |    | (工作节点2)     |    | (工作节点N)     |
+-------+--------+    +-------+--------+    +-------+--------+
        |                     |                     |
        |  HTTP协议(5秒周期)    |                     |
        +---------------------+---------------------+
                              |
                     +--------v---------+
                     |     Manager      |
                     | (中心管理节点)     |
                     +--------+---------+
                              |
                     +--------v---------+
                     |  SQLite数据库     |
                     | (资源数据存储)     |
                     +------------------+
```

## 2. Agent模块设计

### 2.1 模块结构

Agent由以下几个主要模块组成：

1. **资源采集模块**：负责采集本地资源信息
   - CPU采集器
   - 内存采集器
   - 磁盘采集器
   - 网络采集器
   - Docker采集器

2. **HTTP客户端模块**：负责与Manager通信
   - 注册功能
   - 数据上报功能

3. **定时器模块**：控制资源采集和上报的周期

4. **配置管理模块**：管理Agent的配置信息

### 2.2 工作流程

1. Agent启动时，读取配置信息
2. 向Manager发送注册请求，提供自身标识信息
3. 每5秒执行一次资源采集
4. 将采集到的资源信息通过HTTP协议上报给Manager
5. 循环执行步骤3-4

## 3. Manager模块设计

### 3.1 模块结构

Manager由以下几个主要模块组成：

1. **HTTP服务器模块**：提供HTTP接口，接收Agent的注册和数据上报请求
   - Agent注册接口
   - 数据接收接口
   - API查询接口（为未来UI做准备）

2. **Agent管理模块**：管理已注册的Agent信息
   - Agent注册管理
   - Agent状态监控

3. **数据处理模块**：处理接收到的资源数据
   - 数据解析
   - 数据验证

4. **数据存储模块**：将处理后的数据存储到SQLite数据库

### 3.2 工作流程

1. Manager启动时，初始化HTTP服务器和数据库
2. 接收并处理Agent的注册请求，记录Agent信息
3. 接收Agent上报的资源数据
4. 解析和验证数据
5. 将数据存储到SQLite数据库
6. 提供API接口，供未来的Web UI查询数据

## 4. 通信协议设计

Agent和Manager之间采用HTTP协议通信，具体接口如下：

### 4.1 Agent注册接口

- **URL**: `/api/register`
- **方法**: POST
- **请求体**:
  ```json
  {
    "agent_id": "唯一标识符",
    "hostname": "主机名",
    "ip_address": "IP地址",
    "os_info": "操作系统信息"
  }
  ```
- **响应**:
  ```json
  {
    "status": "success/error",
    "message": "成功/错误信息",
    "agent_id": "服务器确认的唯一标识符"
  }
  ```

### 4.2 资源数据上报接口

- **URL**: `/api/report`
- **方法**: POST
- **请求体**:
  ```json
  {
    "agent_id": "唯一标识符",
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

## 5. 数据库设计

SQLite数据库包含以下表：

### 5.1 agents表

存储已注册的Agent信息

```sql
CREATE TABLE agents (
    agent_id TEXT PRIMARY KEY,
    hostname TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    os_info TEXT NOT NULL,
    first_seen TIMESTAMP NOT NULL,
    last_seen TIMESTAMP NOT NULL
);
```

### 5.2 cpu_metrics表

存储CPU资源信息

```sql
CREATE TABLE cpu_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    usage_percent REAL NOT NULL,
    load_avg_1m REAL NOT NULL,
    load_avg_5m REAL NOT NULL,
    load_avg_15m REAL NOT NULL,
    core_count INTEGER NOT NULL,
    FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
);
```

### 5.3 memory_metrics表

存储内存资源信息

```sql
CREATE TABLE memory_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    total BIGINT NOT NULL,
    used BIGINT NOT NULL,
    free BIGINT NOT NULL,
    usage_percent REAL NOT NULL,
    FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
);
```

### 5.4 disk_metrics表

存储磁盘资源信息

```sql
CREATE TABLE disk_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    device TEXT NOT NULL,
    mount_point TEXT NOT NULL,
    total BIGINT NOT NULL,
    used BIGINT NOT NULL,
    free BIGINT NOT NULL,
    usage_percent REAL NOT NULL,
    FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
);
```

### 5.5 network_metrics表

存储网络资源信息

```sql
CREATE TABLE network_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    interface TEXT NOT NULL,
    rx_bytes BIGINT NOT NULL,
    tx_bytes BIGINT NOT NULL,
    rx_packets BIGINT NOT NULL,
    tx_packets BIGINT NOT NULL,
    rx_errors INTEGER NOT NULL,
    tx_errors INTEGER NOT NULL,
    FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
);
```

### 5.6 docker_metrics表

存储Docker容器信息

```sql
CREATE TABLE docker_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id TEXT NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    container_count INTEGER NOT NULL,
    running_count INTEGER NOT NULL,
    paused_count INTEGER NOT NULL,
    stopped_count INTEGER NOT NULL,
    FOREIGN KEY (agent_id) REFERENCES agents(agent_id)
);
```

### 5.7 docker_containers表

存储Docker容器详细信息

```sql
CREATE TABLE docker_containers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    docker_metric_id INTEGER NOT NULL,
    container_id TEXT NOT NULL,
    name TEXT NOT NULL,
    image TEXT NOT NULL,
    status TEXT NOT NULL,
    cpu_percent REAL NOT NULL,
    memory_usage BIGINT NOT NULL,
    FOREIGN KEY (docker_metric_id) REFERENCES docker_metrics(id)
);
```

## 6. 技术选型

### 6.1 第三方库

- **HTTP通信**：使用cpp-httplib库（轻量级HTTP客户端/服务器库）
- **JSON处理**：使用nlohmann/json库（现代C++ JSON库）
- **SQLite操作**：使用SQLiteCpp库（SQLite的C++封装）
- **系统资源获取**：
  - Linux系统调用和/proc文件系统
  - libcurl（可选，用于HTTP请求）
  - Docker API（通过HTTP与Docker守护进程通信）

### 6.2 编译与构建

- 使用CMake作为构建系统
- 支持C++14标准
- 针对Linux平台优化

## 7. 未来扩展

虽然当前不实现，但系统设计考虑了以下未来扩展点：

1. **Web UI**：为Manager添加Web服务器功能，提供资源监控的可视化界面
2. **数据聚合**：支持数据的时间维度聚合，减少存储空间占用
3. **告警机制**：当资源使用超过阈值时，发送告警通知
4. **安全性**：添加认证和加密机制，保护通信安全
5. **配置管理**：支持动态调整资源采集周期和采集项
