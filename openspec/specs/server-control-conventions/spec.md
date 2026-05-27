## Purpose

AI 服务器控制规范，约束 AI 智能体对 farm_demo 服务器进程的操作行为，确保操作一致性、安全性和可追溯性。

## Requirements

### Requirement: PID 文件管理
系统 SHALL 提供 PID 文件管理功能，用于读取和管理服务器进程的 PID 文件。

#### Scenario: 读取 PID 文件
- **WHEN** 调用 `read_pid_file(service_name)`
- **THEN** 返回指定服务的 PID 文件内容

#### Scenario: 检查进程存活
- **WHEN** 调用 `check_process_alive(pid)`
- **THEN** 返回进程是否存活的状态

#### Scenario: 获取服务状态
- **WHEN** 调用 `get_service_status(service_name)`
- **THEN** 返回服务的运行状态（运行中/已停止）

### Requirement: 日志读取和解析
系统 SHALL 提供日志读取和解析功能，用于分析服务器运行日志。

#### Scenario: 读取服务器日志
- **WHEN** 调用 `read_server_logs(service_name, lines=100)`
- **THEN** 返回指定服务的最近日志行

#### Scenario: 解析日志行
- **WHEN** 调用 `parse_log_line(log_line)`
- **THEN** 返回解析后的日志信息（时间、级别、模块等）

#### Scenario: 按级别过滤日志
- **WHEN** 调用 `filter_logs_by_level(logs, level)`
- **THEN** 返回指定级别的日志条目

#### Scenario: 按模块过滤日志
- **WHEN** 调用 `filter_logs_by_module(logs, module)`
- **THEN** 返回指定模块的日志条目

#### Scenario: 检查错误日志
- **WHEN** 调用 `check_error_logs(service_name)`
- **THEN** 返回服务的错误日志条目

### Requirement: 优雅停服
系统 SHALL 提供优雅停服功能，通过发送关闭消息来停止服务器。

#### Scenario: 发送关闭消息
- **WHEN** 调用 `send_shutdown_message(host, port, timeout_ms=5000)`
- **THEN** 向指定服务发送 MSG_ID_SHUTDOWN 消息

#### Scenario: 优雅停服单个服务
- **WHEN** 调用 `graceful_shutdown_service(service_name, timeout_ms=10000)`
- **THEN** 优雅停服指定服务

#### Scenario: 优雅停服所有服务
- **WHEN** 调用 `graceful_shutdown_all(timeout_ms=10000)`
- **THEN** 按逆序优雅停服所有服务

### Requirement: 强制停服
系统 SHALL 提供强制停服功能，用于在优雅停服失败时强制终止进程。

#### Scenario: 强制终止进程
- **WHEN** 调用 `force_kill_process(pid)`
- **THEN** 使用 taskkill 强制终止指定进程

#### Scenario: 强制停服单个服务
- **WHEN** 调用 `force_kill_service(service_name)`
- **THEN** 强制停服指定服务

#### Scenario: 清理 PID 文件
- **WHEN** 调用 `cleanup_pid_file(service_name)`
- **THEN** 清理指定服务的 PID 文件

### Requirement: 启动功能
系统 SHALL 提供启动功能，用于按顺序启动服务器。

#### Scenario: 启动单个服务
- **WHEN** 调用 `start_service(service_name, exe_path, delay_after=2)`
- **THEN** 启动指定服务

#### Scenario: 启动所有服务
- **WHEN** 调用 `start_all_services()`
- **THEN** 按顺序启动所有服务（dbmgr -> game_server -> gate_server）

### Requirement: 重启功能
系统 SHALL 提供重启功能，用于重启所有服务器。

#### Scenario: 重启所有服务
- **WHEN** 调用 `restart_all(timeout_ms=10000)`
- **THEN** 重启所有服务（先停后起）

### Requirement: 操作验证
系统 SHALL 提供操作验证功能，用于验证启动和停服操作是否成功。

#### Scenario: 验证启动操作
- **WHEN** 调用 `verify_startup()`
- **THEN** 验证启动操作（PID 文件、进程存活、无启动错误）

#### Scenario: 验证停服操作
- **WHEN** 调用 `verify_shutdown()`
- **THEN** 验证停服操作（进程已退出、PID 文件已清理）
