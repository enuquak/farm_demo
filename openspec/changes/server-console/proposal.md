## Why

当前服务器生命周期管理依赖 Windows batch 脚本（`tools/start_all.bat`、`stop_graceful.bat`、`stop_force.bat`），各服务器在独立窗口中运行，日志分散在各自的 `.log` 文件中，缺乏统一的实时监控界面。开发和测试过程中需要频繁启停服务器、排查日志，手动操作效率低下。同时，AI 智能体在自动化测试时缺乏标准化的服务器控制规范，容易出现不一致的操作行为。

## What Changes

- **新增 Python GUI 服务器控制台**：基于 CustomTkinter 构建，提供一键重启、强制停服、实时日志查看功能
- **新增 server-control-conventions skill**：约束 AI 智能体对服务器的操作行为，统一重启流程和日志读取规范
- **保留现有 batch 脚本**：不删除原有工具，控制台作为新工具并行存在

## Capabilities

### New Capabilities

- `server-console-gui`: Python GUI 控制台工具，包含进程管理（重启/强制停服）、日志文件尾部读取与缓存、多页签 UI 展示。支持 dbmgr、game_server、gate_server 三个进程，以及未来多个 game_server 实例的动态管理。
- `server-control-conventions`: AI 服务器控制规范 skill，定义重启流程（先停后起）、停服优先级（优雅 > 强制）、日志读取路径与格式、PID 文件管理规则、依赖顺序约束。

### Modified Capabilities

- `server-lifecycle`: 新增 Python 控制台作为替代操作入口，现有 batch 脚本行为不变，但 spec 中需补充 Python 控制台的重启流程描述。

## Impact

- **新增依赖**：`customtkinter`（pip install），Python 3.12 标准库已包含 `tkinter`、`subprocess`、`threading`、`collections.deque`
- **新增目录**：`tools/server_console/`（Python 包）、`.claude/skills/server-control-conventions/`（skill）
- **文件新增**：
  - `tools/server_console/main.py` — 入口
  - `tools/server_console/server_manager.py` — 进程管理
  - `tools/server_console/log_reader.py` — 日志读取
  - `tools/server_console/gui/app.py` — 主窗口
  - `tools/server_console/gui/toolbar.py` — 按钮栏
  - `tools/server_console/gui/log_panel.py` — 日志面板
  - `.claude/skills/server-control-conventions/SKILL.md` — AI 控制规范
- **不受影响**：C++ 服务器代码、现有 batch 脚本、客户端代码均无修改
