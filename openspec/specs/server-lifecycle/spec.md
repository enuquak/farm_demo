## Purpose

Server Lifecycle 模块负责管理各服务进程（dbmgr、gate_server、game_server）的启动顺序、PID 文件管理、优雅停服和强制停服功能。通过管理消息协议实现级联停服，并提供一键式工具脚本。

## Requirements

### Requirement: 按顺序启动服务

系统 SHALL 按照依赖顺序启动所有服务：dbmgr → game_server → gate_server。

#### Scenario: 一键启动所有服务
- **WHEN** 执行 `tools/start_all.bat`
- **THEN** 脚本 SHALL 按以下顺序启动服务:
  1. 启动 dbmgr
  2. 等待 2 秒
  3. 启动 game_server
  4. 等待 2 秒
  5. 启动 gate_server
  6. 输出 "All servers started"

#### Scenario: 服务启动命令
- **WHEN** 启动单个服务
- **THEN** 命令格式 SHALL 为: `bin\<service_name>.exe --config config\<service_name>.json`

### Requirement: PID 文件管理

各服务启动后 SHALL 将自身 PID 写入配置文件指定的 pid_file 路径。

#### Scenario: PID 文件写入
- **WHEN** 服务成功启动
- **THEN** 服务 SHALL 将当前进程 ID 写入 pid_file 指定的路径
- **AND** PID 文件内容 SHALL 仅为数字，无其他字符

#### Scenario: PID 文件覆盖
- **WHEN** 服务启动且 pid_file 已存在
- **THEN** 服务 SHALL 覆盖写入新的 PID

#### Scenario: runtimeData 目录
- **WHEN** pid_file 路径包含 `runtimeData/` 目录
- **THEN** 服务 SHALL 确保该目录存在后再写入 PID 文件

### Requirement: 优雅停服（消息方式）

系统 SHALL 支持通过发送管理消息实现优雅停服。

#### Scenario: 优雅停服流程
- **WHEN** 执行 `tools/stop_graceful.bat`
- **THEN** 脚本 SHALL 连接到 gate_server 并发送 MSG_ID_SHUTDOWN 消息
- **AND** gate_server SHALL 收到消息后:
  1. 停止接受新连接
  2. 向所有 game_server 发送 MSG_ID_SHUTDOWN
  3. 等待响应或超时
- **AND** game_server SHALL 收到消息后:
  1. 停止接受新连接
  2. 向所有 dbmgr 发送 MSG_ID_SHUTDOWN
  3. 等待响应或超时
- **AND** dbmgr SHALL 收到消息后:
  1. 保存所有数据
  2. 发送 MSG_ID_SHUTDOWN_RESP
  3. 退出

#### Scenario: 优雅停服超时
- **WHEN** 优雅停服超时（默认 60 秒）
- **THEN** 脚本 SHALL 强制终止未退出的进程

### Requirement: 管理消息协议

系统 SHALL 使用消息 ID 范围 5000-5999 作为管理消息。

#### Scenario: MSG_ID_SHUTDOWN 消息
- **WHEN** 发送停服消息
- **THEN** 消息 ID SHALL 为 5001
- **AND** 消息体 SHALL 包含:
  - `reason`: 停服原因，字符串类型
  - `timeout_ms`: 建议超时时间，整数类型

#### Scenario: MSG_ID_SHUTDOWN_RESP 消息
- **WHEN** 响应停服消息
- **THEN** 消息 ID SHALL 为 5002
- **AND** 消息体 SHALL 包含:
  - `code`: 响应码，0 表示准备就绪
  - `msg`: 响应消息，字符串类型

### Requirement: 强制停服

系统 SHALL 支持通过 PID 文件强制终止服务。

#### Scenario: 强制停服流程
- **WHEN** 执行 `tools/stop_force.bat`
- **THEN** 脚本 SHALL:
  1. 读取各服务的 PID 文件
  2. 使用 `taskkill /PID <pid> /F` 强制终止进程
  3. 输出终止结果

#### Scenario: PID 文件不存在
- **WHEN** 强制停服时 PID 文件不存在
- **THEN** 脚本 SHALL 输出警告信息并继续处理其他服务

### Requirement: 工具脚本

系统 SHALL 在 `tools/` 目录提供以下脚本:

#### Scenario: build_all.bat
- **WHEN** 执行 `tools/build_all.bat`
- **THEN** 脚本 SHALL 编译所有服务并输出到 `bin/` 目录

#### Scenario: start_all.bat
- **WHEN** 执行 `tools/start_all.bat`
- **THEN** 脚本 SHALL 按顺序启动所有服务

#### Scenario: stop_graceful.bat
- **WHEN** 执行 `tools/stop_graceful.bat`
- **THEN** 脚本 SHALL 执行优雅停服流程

#### Scenario: stop_force.bat
- **WHEN** 执行 `tools/stop_force.bat`
- **THEN** 脚本 SHALL 执行强制停服流程
