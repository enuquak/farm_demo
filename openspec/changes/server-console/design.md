## Context

当前服务器生命周期由 `tools/` 下的 Windows batch 脚本管理：`start_all.bat` 按依赖顺序（dbmgr → game_server → gate_server）逐个启动，每个服务器在独立 cmd 窗口中运行；`stop_graceful.bat` 通过 TCP 发送 MSG_ID_SHUTDOWN 给 gate_server 触发级联停服；`stop_force.bat` 读取 PID 文件后 `taskkill /F`。

日志由 spdlog 写入 `runtimeData/logs/server/{name}.log`，格式为 `[YYYY-MM-DD HH:MM:SS.mmm][level][name:pid] [Module]message`，每次写入立即 flush。

**约束**：
- 不修改 C++ 服务器代码
- 不删除现有 batch 脚本
- Python 3.12 可用，无 tkinter（需 pip install customtkinter）
- Windows 11 环境

## Goals / Non-Goals

**Goals:**
- 提供统一的 GUI 界面管理所有服务器进程的生命周期
- 实时显示每个服务器的日志输出，支持多页签切换
- 一键重启（先停后起），替代手动执行多个 batch 脚本
- 为 AI 智能体提供标准化的服务器控制规范

**Non-Goals:**
- 不实现服务器健康检查/心跳探活（后续迭代）
- 不实现服务器崩溃自动重启（watchdog）
- 不支持远程服务器管理（仅本地）
- 不替换现有 batch 脚本（并行共存）
- 不支持单个服务器独立启动（只有 Restart All）

## Decisions

### Decision 1: GUI 框架选择 CustomTkinter

**选择**: CustomTkinter (pip install customtkinter)

**理由**:
- 基于 tkinter，Python 标准库自带底层支持，无需额外大型依赖
- 原生深色主题，圆角组件，外观现代
- API 是 tkinter 的超集，学习成本低
- 轻量级（~2MB），适合工具类应用

**考虑过的替代方案**:
- PyQt6/PySide6：功能最强但依赖 ~80MB，对工具来说过重
- ttkbootstrap：仍是 tkinter 的皮肤，外观提升有限
- textual：终端 TUI，非 GUI

### Decision 2: 进程管理策略 — subprocess + PID 文件

**选择**: 使用 `subprocess.Popen` 启动服务器进程，同时保留 PID 文件机制。

**架构**:
```
ServerManager
├── start_server(name) → Popen + 等待 PID 文件写入
├── stop_server(name)  → TCP MSG_ID_SHUTDOWN → 等待进程退出 → fallback taskkill
├── force_stop(name)   → taskkill /PID /F + 清理 PID 文件
├── restart_all()      → stop_all(reverse=True) → wait → start_all(顺序)
└── get_status(name)   → 检查 PID 文件 + psutil/pid 存活检测
```

**重启流程**:
```
restart_all():
  1. stop gate_server  (优雅 → 超时 → force)
  2. stop game_server  (优雅 → 超时 → force)
  3. stop dbmgr        (优雅 → 超时 → force)
  4. 确认全部停止
  5. start dbmgr       → 等待 2s → 验证 PID
  6. start game_server → 等待 2s → 验证 PID
  7. start gate_server → 验证 PID
  8. 完成
```

**理由**: subprocess 直接管理子进程更可靠，PID 文件作为辅助状态用于跨工具兼容（batch 脚本也能读取）。

### Decision 3: 日志读取 — 文件 tail + deque 缓存

**选择**: 后台线程持续 tail 读取日志文件，数据写入 `deque(maxlen=10000)` 缓存。

**架构**:
```
LogReader (per server)
├── _tail_thread: 后台线程，循环 read 新增行
├── cache: deque(maxlen=10000)  ← 内存缓存
├── callbacks: list[callable]   ← 新行回调（通知 GUI）
└── start() / stop()

GUI 切换 tab:
  1. 暂停当前 tab 的显示更新
  2. 清空 Textbox
  3. 从目标 server 的 LogReader.cache 回放全部行
  4. 恢复自动滚动
```

**理由**:
- 不需要修改 C++ 代码（spdlog 已配置每次 flush）
- deque 有界，内存可控（10000 行 ≈ 2-3MB）
- 切 tab 时从缓存回放，无需重新读文件

### Decision 4: 模块结构

```
tools/server_console/
├── __init__.py
├── main.py              # 入口，解析参数，启动 App
├── server_manager.py    # ServerManager 类：进程生命周期
├── log_reader.py        # LogReader 类：tail + cache + callback
└── gui/
    ├── __init__.py
    ├── app.py           # CTk 主窗口，组合 Toolbar + LogPanel
    ├── toolbar.py       # 按钮栏：Restart All / Force Stop / 状态指示
    └── log_panel.py     # CTkTabview，每个 tab 包含 CTkTextbox + 自动滚动
```

**职责划分**:
- `server_manager.py` — 纯逻辑，不依赖 GUI，可被 AI skill 脚本直接调用
- `log_reader.py` — 纯逻辑，后台线程 + 缓存
- `gui/` — 纯 UI，通过回调与逻辑层交互

### Decision 5: AI Skill 定位

Skill 不直接调用 GUI，而是通过 Python 脚本调用 `ServerManager` 的方法：

```
AI 需要重启服务器时:
  1. 检查是否有控制台 GUI 在运行（PID 文件）
  2. 如果有 → 通过 GUI 的 Restart All 按钮
  3. 如果没有 → 直接调用 server_manager.restart_all()
  4. 验证：检查 PID 文件 + 端口监听
```

Skill 重点约束：
- **必须先停后起**，禁止直接 start
- **停服优先优雅**，超时才 force
- **日志读取**：路径 `runtimeData/logs/server/{name}.log`
- **PID 文件**：路径 `runtimeData/{name}.pid`

## Risks / Trade-offs

**[Risk] CustomTkinter 兼容性**
→ CustomTkinter 依赖 tkinter，在部分精简 Python 安装中可能缺失。
→ Mitigation: 安装时检查 tkinter 可用性，给出明确错误提示。

**[Risk] 日志文件轮转（spdlog rotating）**
→ spdlog 配置了 max_file_size=20MB, max_files=7，文件轮转时 tail 读取可能丢失行。
→ Mitigation: LogReader 监控文件 inode 变化（Windows 上监控文件名变化），轮转时重新打开文件。

**[Risk] 优雅停服 TCP 连接失败**
→ 如果 gate_server 未启动或端口未监听，TCP 连接会失败。
→ Mitigation: 连接失败直接 fallback 到 force stop。

**[Risk] 多 game_server 实例管理**
→ 当前设计支持多个 game_server，但配置和日志文件命名需要约定。
→ Mitigation: 初期只支持 1 个 game_server，后续迭代时在 config 中增加 game_server 列表支持。

**[Trade-off] 缓存大小 vs 内存**
→ 10000 行缓存约 2-3MB，对开发工具可接受。如果日志量极大可调整 maxlen。

**[Trade-off] 不自动重启 vs 便利性**
→ 不实现 watchdog 自动重启，简化设计。开发时手动重启即可。
