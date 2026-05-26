## Why

当前 dbmgr、gate_server、game_server 三个进程的启动配置分散在命令行参数中，gate_server 甚至使用位置参数，缺乏规范性。没有统一的配置管理、编译输出目录、以及启动/停止脚本，导致部署和运维不便。

## What Changes

- **新增 `config/` 目录**：按进程拆分 JSON 配置文件（dbmgr.json、gate_server.json、game_server.json），统一管理端口、数据库 URI 等配置项
- **新增 `bin/` 目录**：统一编译输出，三个服务的可执行文件和 DLL 集中存放
- **新增 `runtimeData/` 目录**：存放运行时数据（PID 文件等）
- **新增 `tools/` 目录**：存放 build_all.bat、start_all.bat、stop_graceful.bat、stop_force.bat 脚本
- **集成 nlohmann/json 库**：header-only JSON 解析库，用于 C++ 服务读取配置文件
- **新增优雅停服消息协议**：管理消息 ID 范围 5000-5999，支持 MSG_ID_SHUTDOWN / MSG_ID_SHUTDOWN_RESP
- **修改各服务 main.cpp**：从 JSON 配置文件读取启动参数，支持 PID 文件写入

## Capabilities

### New Capabilities

- `server-config`：服务器配置管理，定义各进程 JSON 配置文件格式和加载机制
- `server-lifecycle`：服务器生命周期管理，包括按顺序启动、优雅停服、强制停服

### Modified Capabilities

（无现有 capability 需要修改）

## Impact

- **源码变更**：scripts/server/{dbmgr,game_server,gate_server}/src/main.cpp 需要修改以支持 JSON 配置加载
- **构建系统**：三个 CMakeLists.txt 需要修改输出目录到 bin/，并添加 nlohmann/json 的 include 路径
- **新增依赖**：nlohmann/json (header-only，无需编译)
- **新增文件**：
  - config/*.json（3 个配置文件）
  - scripts/common/include/nlohmann/json.hpp
  - tools/*.bat（4 个脚本）
  - scripts/common/proto/admin.proto（停服消息定义）
