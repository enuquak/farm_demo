## MODIFIED Requirements

### Requirement: 工具脚本

系统 SHALL 在 `tools/` 目录提供以下脚本及工具:

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

#### Scenario: Python 服务器控制台
- **WHEN** 执行 `python -m tools.server_console`
- **THEN** 系统 SHALL 启动 GUI 控制台
- **AND** 控制台 SHALL 提供 Restart All 和 Force Stop 按钮
- **AND** 控制台 SHALL 实时显示各服务日志

#### Scenario: Python 控制台重启流程
- **WHEN** 用户在控制台点击 Restart All
- **THEN** 控制台 SHALL 执行以下步骤:
  1. 逆序停止：gate_server → game_server → dbmgr
  2. 每个服务先尝试优雅停服（MSG_ID_SHUTDOWN），超时后强制终止
  3. 等待所有进程退出
  4. 正序启动：dbmgr → game_server → gate_server
  5. 每步间隔 2 秒并验证 PID 文件
