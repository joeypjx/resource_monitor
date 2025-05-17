# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(resource_monitor)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra)

# 查找必要的包
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json QUIET)

# 如果没有找到nlohmann_json，则使用FetchContent下载
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# 添加SQLiteCpp
include(FetchContent)
FetchContent_Declare(
    sqlitecpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG 3.2.1
)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sqlitecpp)

# 添加cpp-httplib
FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.12.0
)
FetchContent_MakeAvailable(cpp_httplib)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/manager
    ${CURL_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${cpp_httplib_SOURCE_DIR}
)

# Agent源文件
set(AGENT_SOURCES
    src/agent/agent.cpp
    src/agent/cpu_collector.cpp
    src/agent/memory_collector.cpp
    src/agent/disk_collector.cpp
    src/agent/network_collector.cpp
    src/agent/docker_collector.cpp
    src/agent/http_client.cpp
)

# Manager源文件
set(MANAGER_SOURCES
    src/manager/manager.cpp
    src/manager/http_server.cpp
    src/manager/database_manager.cpp
)

# Agent可执行文件
add_executable(agent src/agent_main.cpp ${AGENT_SOURCES})
target_link_libraries(agent
    PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    uuid
)

# Manager可执行文件
add_executable(manager src/manager_main.cpp ${MANAGER_SOURCES})
target_link_libraries(manager
    PRIVATE
    Threads::Threads
    SQLite::SQLite3
    SQLiteCpp
    nlohmann_json::nlohmann_json
)

# 安装目标
install(TARGETS agent manager
    RUNTIME DESTINATION bin
)
