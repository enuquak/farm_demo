# Server Lifecycle 开发任务

## 任务总览

根据 spec 要求分析，大部分 server-lifecycle 功能已在项目中实现。本次开发重点是补全 --config 参数解析支持。

## 任务列表

### 1. 服务启动命令格式修正

#### 1.1 修改 gate_server main.cpp 支持 --config 参数 [已完成 ✅]

- **目标**: 修改 `scripts/server/gate_server/src/main.cpp`，支持 `--config <path>` 命令行参数
- **实现**: 在 main() 中解析 --config 参数，保留默认路径作为 fallback

#### 1.2 修改 game_server main.cpp 支持 --config 参数 [已完成 ✅]

- **目标**: 修改 `scripts/server/game_server/src/main.cpp`，支持 `--config <path>` 命令行参数
- **实现**: 同上

#### 1.3 修改 dbmgr main.cpp 支持 --config 参数 [已完成 ✅]

- **目标**: 修改 `scripts/server/dbmgr/src/main.cpp`，支持 `--config <path>` 命令行参数
- **实现**: 同上，同时移除了不再使用的 get_config_path() 函数

#### 1.4 验证 start_all.bat 使用 --config 标志 [已完成 ✅]

- **目标**: 确认 `tools/start_all.bat` 使用 `--config` 标志传递配置路径
- **结果**: 已确认 start_all.bat 已正确使用 `--config` 格式

### 2. 优雅停服级联流程验证

#### 2.1 验证 gate_server 优雅停服实现 [已完成 ✅]

- **目标**: 检查 `gate_server.cpp` 的 `handle_shutdown` 实现是否符合 spec
- **spec 要求**: 停止接受新连接 -> 向所有 game_server 发送 MSG_ID_SHUTDOWN -> 等待响应或超时
- **结果**: 已正确实现（evconnlistener_disable + 转发 MSG_ID_SHUTDOWN 到所有 Game Server + stop()）

#### 2.2 验证 game_server 优雅停服实现 [已完成 ✅]

- **目标**: 检查 `game_server.cpp` 的 `handle_shutdown` 实现是否符合 spec
- **spec 要求**: 停止接受新连接 -> 保存玩家数据 -> 向 dbmgr 发送 MSG_ID_SHUTDOWN -> 等待响应或超时
- **结果**: 已正确实现（evconnlistener_disable + save_all_players + 转发 MSG_ID_SHUTDOWN 到 DBMgr + stop()）

#### 2.3 验证 dbmgr 优雅停服实现 [已完成 ✅]

- **目标**: 检查 `dbmgr_server.cpp` 的 `handle_shutdown` 实现是否符合 spec
- **spec 要求**: 保存所有数据 -> 发送 MSG_ID_SHUTDOWN_RESP -> 退出
- **结果**: 已正确实现（DataManager 持久化 + 发送 MSG_ID_SHUTDOWN_RESP + stop()）

### 3. 编译验证

#### 3.1 编译 gate_server [已完成 ✅]

- **结果**: 编译成功，输出 bin/gate_server.exe (998912 bytes)
- **DLL 依赖**: event.dll, event_core.dll, event_extra.dll 均已存在

#### 3.2 编译 game_server [已完成 ✅]

- **结果**: 编译成功，输出 bin/game_server.exe (1099264 bytes)
- **DLL 依赖**: 同上

#### 3.3 编译 dbmgr [已完成 ✅]

- **结果**: 编译成功，输出 bin/dbmgr.exe (1025024 bytes)
- **DLL 依赖**: 同上

#### 3.4 运行 build_all.bat 验证 [已完成 ✅]

- **结果**: 三个服务均编译成功，bin/ 目录内容完整
