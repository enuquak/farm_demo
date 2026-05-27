# server-console-gui Task List

## 1. 创建项目目录结构
- [x] 1.1 创建 `tools/server_console/` 目录 [已完成 ✅]
- [x] 1.2 创建 `__init__.py`、`__main__.py`、`process_manager.py`、`log_reader.py`、`gui.py`、`constants.py` 文件骨架 [已完成 ✅]

## 2. 实现 constants.py - 常量定义
- [x] 2.1 定义服务名称列表（dbmgr, game_server, gate_server） [已完成 ✅]
- [x] 2.2 定义 PID 文件路径模板：`runtimeData/{name}.pid` [已完成 ✅]
- [x] 2.3 定义日志文件路径模板：`runtimeData/logs/server/{name}.log` [已完成 ✅]
- [x] 2.4 定义启动命令模板：`bin/{name}.exe --config config/{name}.json` [已完成 ✅]
- [x] 2.5 定义状态检测间隔（2 秒）、日志重试间隔（1 秒）、启动验证超时（5 秒） [已完成 ✅]

## 3. 实现 process_manager.py - 进程管理模块
- [x] 3.1 实现 `read_pid(name)` - 读取 PID 文件，返回 int 或 None [已完成 ✅]
- [x] 3.2 实现 `is_process_alive(pid)` - 通过 PID 检查进程是否存活 [已完成 ✅]
- [x] 3.3 实现 `get_service_status(name)` - 综合 PID 文件和进程存活检测 [已完成 ✅]
- [x] 3.4 实现 `force_stop_service(name)` - taskkill /PID /F 强制终止单个服务 [已完成 ✅]
- [x] 3.5 实现 `force_stop_all()` - 按逆序强制终止所有服务并清理 PID 文件 [已完成 ✅]
- [x] 3.6 实现 `start_service(name)` - subprocess.Popen 启动单个服务，等待 PID 文件写入（5 秒超时） [已完成 ✅]
- [x] 3.7 实现 `restart_all()` - 逆序停止 + 正序启动，每步间隔 2 秒 [已完成 ✅]

## 4. 实现 log_reader.py - 日志读取模块
- [x] 4.1 实现 `LogReader` 类，后台线程持续 tail 日志文件 [已完成 ✅]
- [x] 4.2 实现 `deque(maxlen=10000)` 缓存机制 [已完成 ✅]
- [x] 4.3 实现文件不存在时每 1 秒重试检测 [已完成 ✅]
- [x] 4.4 实现日志文件轮转检测（文件 inode/路径变化时重新打开） [已完成 ✅]
- [x] 4.5 实现 GUI 通知回调（新行到达时调用 callback） [已完成 ✅]

## 5. 实现 gui.py - GUI 界面模块
- [x] 5.1 创建 CustomTkinter 主窗口，标题 "Server Console" [已完成 ✅]
- [x] 5.2 实现工具栏：Restart All 按钮 + Force Stop 按钮 + 状态文本 "Status: X/3" [已完成 ✅]
- [x] 5.3 实现 TabView 日志页签（dbmgr, game_server, gate_server） [已完成 ✅]
- [x] 5.4 实现日志显示区域（CTkTextbox，只读，monospace 字体） [已完成 ✅]
- [x] 5.5 实现自动滚动逻辑（到底部自动滚动，用户上滚暂停，回到底部恢复） [已完成 ✅]
- [x] 5.6 实现页签切换时清空显示并从 LogReader 缓存回放 [已完成 ✅]
- [x] 5.7 实现状态检测定时器（每 2 秒检测 PID 文件，更新页签标题和状态栏） [已完成 ✅]
- [x] 5.8 实现按钮状态锁定（重启中禁用 Restart All，Force Stop 保持可用） [已完成 ✅]

## 6. 实现 __main__.py - 入口模块
- [x] 6.1 配置 logging 模块 [已完成 ✅]
- [x] 6.2 创建 GUI 实例并启动主循环 [已完成 ✅]
- [x] 6.3 支持 `python -m tools.server_console` 方式启动 [已完成 ✅]

## 7. 验证与测试
- [x] 7.1 验证 CustomTkinter 依赖可安装 [已完成 ✅]
- [x] 7.2 验证 GUI 可正常启动和显示 [已完成 ✅]
- [x] 7.3 验证进程管理逻辑（PID 读取、进程存活检测） [已完成 ✅]
- [x] 7.4 验证日志读取逻辑（文件 tail、缓存、轮转检测） [已完成 ✅]
