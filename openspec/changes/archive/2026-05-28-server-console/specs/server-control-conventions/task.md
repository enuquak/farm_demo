# Server Control Conventions - 实现任务

## 任务拆分

### 1. 创建 server_control.py 模块基础结构
- [已完成 ✅]
- 创建 Python 模块文件 `scripts/common/server_control.py`
- 定义模块常量（PID 文件路径、日志文件路径、服务启动顺序等）
- 实现基础日志配置

### 2. 实现 PID 文件管理功能
- [已完成 ✅]
- `read_pid_file(service_name)` - 读取指定服务的 PID 文件
- `check_process_alive(pid)` - 检查进程是否存活
- `get_service_status(service_name)` - 获取服务状态（运行中/已停止）

### 3. 实现日志读取和解析功能
- [已完成 ✅]
- `read_server_logs(service_name, lines=100)` - 读取服务器日志
- `parse_log_line(log_line)` - 解析日志行，提取时间、级别、模块等
- `filter_logs_by_level(logs, level)` - 按日志级别过滤
- `filter_logs_by_module(logs, module)` - 按模块过滤
- `check_error_logs(service_name)` - 检查错误日志

### 4. 实现优雅停服功能
- [已完成 ✅]
- `send_shutdown_message(host, port, timeout_ms=5000)` - 发送 MSG_ID_SHUTDOWN 消息
- `graceful_shutdown_service(service_name, timeout_ms=10000)` - 优雅停服单个服务
- `graceful_shutdown_all(timeout_ms=10000)` - 优雅停服所有服务（逆序）

### 5. 实现强制停服功能
- [已完成 ✅]
- `force_kill_process(pid)` - 使用 taskkill 强制终止进程
- `force_kill_service(service_name)` - 强制停服单个服务
- `cleanup_pid_file(service_name)` - 清理 PID 文件

### 6. 实现启动功能
- [已完成 ✅]
- `start_service(service_name, exe_path, delay_after=2)` - 启动单个服务
- `start_all_services()` - 按顺序启动所有服务（dbmgr -> game_server -> gate_server）

### 7. 实现重启功能
- [已完成 ✅]
- `restart_all(timeout_ms=10000)` - 重启所有服务（先停后起）
- 实现操作验证逻辑

### 8. 实现操作验证功能
- [已完成 ✅]
- `verify_startup()` - 验证启动操作（PID 文件、进程存活、无启动错误）
- `verify_shutdown()` - 验证停服操作（进程已退出、PID 文件已清理）

### 9. 编写单元测试
- [已完成 ✅]
- 创建测试文件 `scripts/server/tests/test_server_control.py`
- 测试 PID 文件读取
- 测试日志解析
- 测试服务状态检查

## 依赖关系

- 任务 1-3 为基础功能，无依赖
- 任务 4-5 依赖任务 2（PID 管理）
- 任务 6 依赖任务 2（PID 管理）
- 任务 7 依赖任务 4-6（停服和启动）
- 任务 8 依赖任务 2-7（所有功能）
- 任务 9 依赖任务 1-8（所有功能）

## 开发顺序

1. 任务 1: 创建模块基础结构
2. 任务 2: 实现 PID 文件管理
3. 任务 3: 实现日志读取和解析
4. 任务 4: 实现优雅停服
5. 任务 5: 实现强制停服
6. 任务 6: 实现启动功能
7. 任务 7: 实现重启功能
8. 任务 8: 实现操作验证
9. 任务 9: 编写单元测试
