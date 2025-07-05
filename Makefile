# 资源监控系统 Makefile

# 编译器和标准
CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -MMD -MP

# 项目目录
SRC_DIR = src
AGENT_DIR = $(SRC_DIR)/agent
MANAGER_DIR = $(SRC_DIR)/manager
UTILS_DIR = $(SRC_DIR)/utils
BUILD_DIR = build

# 依赖库目录
DEPS_DIR = third_party
JSON_DIR = $(DEPS_DIR)/nlohmann_json
HTTPLIB_DIR = $(DEPS_DIR)/cpp-httplib
SPDLOG_DIR = $(DEPS_DIR)/spdlog
SQLITECPP_DIR = $(DEPS_DIR)/SQLiteCpp
SQLITE3_DIR = $(DEPS_DIR)/sqlite3
# devel 头文件
SSH_DIR = $(DEPS_DIR)/libssh2
UUID_DIR = $(DEPS_DIR)/libuuid
CURL_DIR = $(DEPS_DIR)/curl

# 包含目录
INCLUDES = -I$(SRC_DIR) \
          -I$(AGENT_DIR) \
          -I$(MANAGER_DIR) \
		  -I$(UTILS_DIR) \
		  -I$(DEPS_DIR) \
          -I$(JSON_DIR)/include \
          -I$(HTTPLIB_DIR) \
          -I$(SQLITECPP_DIR)/include \
		  -I$(SQLITE3_DIR)/include \
          -I$(SPDLOG_DIR)/include

# 库目录
LIB_DIRS = -L$(SQLITECPP_DIR)/build \
		   -L$(SQLITE3_DIR)/lib

# VPATH告诉make在哪里查找源文件
VPATH = $(AGENT_DIR):$(MANAGER_DIR):$(UTILS_DIR):$(SRC_DIR)

# Agent源文件
AGENT_SRC_FILES = $(wildcard $(AGENT_DIR)/*.cpp) $(SRC_DIR)/agent_main.cpp
MANAGER_SRC_FILES = $(wildcard $(MANAGER_DIR)/*.cpp) $(SRC_DIR)/manager_main.cpp
COMMON_SRC_FILES = $(wildcard $(UTILS_DIR)/*.cpp)

# 目标文件
AGENT_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(AGENT_SRC_FILES) $(COMMON_SRC_FILES)))
MANAGER_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(MANAGER_SRC_FILES) $(COMMON_SRC_FILES)))

# 依赖库
AGENT_LIBS = -l:libcurl.so.4 -l:libuuid.so.1 -l:libssh2.so.1 -lpthread
MANAGER_LIBS = -l:libuuid.so.1 -lSQLiteCpp $(SQLITE3_DIR)/lib/libsqlite3.a -lpthread -ldl

# 目标可执行文件
AGENT_TARGET = $(BUILD_DIR)/agent
MANAGER_TARGET = $(BUILD_DIR)/manager

# 默认目标
all: $(AGENT_TARGET) $(MANAGER_TARGET)

# 准备构建目录

# 编译Agent
$(AGENT_TARGET): $(AGENT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_DIRS) $(AGENT_LIBS)

# 编译Manager
$(MANAGER_TARGET): $(MANAGER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_DIRS) $(MANAGER_LIBS)

# 编译规则
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# 清理
clean:
	rm -rf $(BUILD_DIR)

# 安装
install: all
	mkdir -p /usr/local/zygl/
	cp $(AGENT_TARGET) /usr/local/zygl/
	cp $(MANAGER_TARGET) /usr/local/zygl/

# 包含自动生成的依赖文件
-include $(AGENT_OBJECTS:.o=.d)
-include $(MANAGER_OBJECTS:.o=.d)

# 依赖检查
deps:
	@echo "正在检查 依赖项..."
	@rpm -q gcc-c++ > /dev/null || (echo "错误: 需要安装 C++ 编译器。请运行: sudo yum install gcc-c++" && exit 1)
	@rpm -q libcurl-devel > /dev/null || (echo "错误: 需要安装 libcurl 开发库。请运行: sudo yum install libcurl-devel" && exit 1)
	@rpm -q uuid-devel > /dev/null || (echo "错误: 需要安装 libuuid 开发库。请运行: sudo yum install uuid-devel" && exit 1)
	@rpm -q libssh2-devel > /dev/null || (echo "错误: 需要安装 libssh2 开发库。请运行: sudo yum install libssh2-devel" && exit 1)
	@echo "所有依赖项均已满足。"

# 帮助
help:
	@echo "资源监控系统 Makefile"
	@echo "使用方法:"
	@echo "  make        - 构建agent和manager"
	@echo "  make agent  - 仅构建agent"
	@echo "  make manager - 仅构建manager"
	@echo "  make clean   - 清理构建文件"
	@echo "  make install - 安装到系统"
	@echo "  make deps    - 检查依赖"
	@echo "  make help    - 显示此帮助信息"
	@echo "  make build-sqlite3 - 重新编译sqlite3库"
	@echo "  make build-sqlitecpp - 重新编译SQLiteCpp库"

# 单独构建目标
agent: $(AGENT_TARGET)
manager: $(MANAGER_TARGET)

# 重新编译sqlite3
build-sqlite3:
	@echo "正在编译 sqlite3..."
	@cd $(SQLITE3_DIR) && \
	(make clean || true) && \
	rm -rf lib include && \
	./configure --prefix=$(CURDIR)/$(SQLITE3_DIR) --enable-static --disable-shared CFLAGS="-DSQLITE_ENABLE_COLUMN_METADATA" && \
	make && \
	make install

# 重新编译 SQLiteCpp
build-sqlitecpp:
	@echo "正在编译 SQLiteCpp..."
	@cd $(SQLITECPP_DIR) && \
	rm -rf build && \
	mkdir -p build && \
	cd build && \
	cmake .. -DSQLITECPP_BUILD_EXAMPLES=OFF -DSQLITECPP_BUILD_TESTS=OFF && \
	make

.PHONY: all clean install deps help agent manager build-sqlitecpp
.DEFAULT_GOAL := all