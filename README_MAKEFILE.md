# 资源监控系统 - Makefile 构建指南

本文档提供了使用 Makefile 构建资源监控系统的说明，替代原有的 CMake 构建系统。

## 系统要求

- C++14 兼容的编译器 (如 g++, clang++)
- libcurl 开发库
- libsqlite3 开发库
- libuuid 开发库
- POSIX 线程库 (pthread)

## 目录结构

```
.
├── src/                  # 源代码目录
│   ├── agent/           # Agent 相关源码
│   ├── manager/         # Manager 相关源码
│   ├── agent_main.cpp   # Agent 主程序
│   └── manager_main.cpp # Manager 主程序
├── deps/                # 依赖库
│   ├── nlohmann_json/   # JSON 处理库
│   ├── cpp-httplib/     # HTTP 客户端/服务器库
│   └── SQLiteCpp/       # SQLite C++ 封装库
├── Makefile             # 新的构建系统
└── README_MAKEFILE.md   # 本文档
```

## 构建说明

### 检查依赖

在构建前，可以检查系统是否满足所有依赖要求：

```bash
make deps
```

### 构建全部组件

```bash
make
```

构建完成后，可执行文件将位于 `build/` 目录中。

### 单独构建组件

仅构建 Agent：

```bash
make agent
```

仅构建 Manager：

```bash
make manager
```

### 清理构建文件

```bash
make clean
```

### 安装到系统

```bash
sudo make install
```

## 运行说明

### 启动 Manager

```bash
./build/manager --port 8080 --db-path resource_monitor.db
```

参数说明：
- `--port <端口>`: 指定 HTTP 服务器监听端口（默认：8080）
- `--db-path <路径>`: 指定数据库文件路径（默认：resource_monitor.db）

### 启动 Agent

```bash
./build/agent --manager-url http://localhost:8080 --interval 5
```

参数说明：
- `--manager-url <URL>`: 指定 Manager 的 URL 地址（默认：http://localhost:8080）
- `--hostname <主机名>`: 覆盖默认主机名
- `--interval <秒>`: 设置资源收集间隔，单位为秒（默认：5）

## 从 CMake 迁移说明

本 Makefile 替代了原有的 CMake 构建系统，主要变化如下：

1. 不再需要 CMake 和相关工具
2. 依赖库的处理方式改变，现在直接使用系统安装的库
3. 构建目录结构简化

如果您之前使用 CMake 构建，请先清理 CMake 生成的文件：

```bash
rm -rf build CMakeFiles CMakeCache.txt
```

然后按照上述说明使用 Makefile 构建系统。

## 故障排除

如果构建过程中遇到问题，请检查：

1. 是否已安装所有必要的依赖库
2. 编译器版本是否支持 C++14
3. 依赖库的头文件和库文件路径是否正确

如需更多帮助，请运行：

```bash
make help
```