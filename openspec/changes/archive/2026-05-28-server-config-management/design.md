## Context

当前 farm_demo 项目有三个服务进程：dbmgr、gate_server、game_server。它们的启动配置方式不统一：
- dbmgr 使用 `--index`, `--port`, `--data-dir` 命令行参数
- gate_server 使用位置参数（argv[1]=port, argv[2]=ip...），不规范
- game_server 使用 `--port`, `--ip`, `--dbmgr` 命令行参数

编译输出分散在各自的 `Release/` 目录，没有统一的启动/停止脚本。

## Goals / Non-Goals

**Goals:**
- 统一配置管理：使用 JSON 配置文件替代命令行参数
- 统一编译输出：所有可执行文件输出到 `bin/` 目录
- 提供一键启动/停止脚本
- 支持优雅停服（消息方式）和强制停服（PID + taskkill）

**Non-Goals:**
- 不实现热重载配置
- 不实现配置文件的运行时修改
- 不支持 Linux/macOS 脚本（本次仅 Windows）
- 不实现服务发现机制

## Decisions

### 1. 配置格式：JSON

**选择**: JSON (nlohmann/json)

**备选方案**:
| 格式 | 优点 | 缺点 |
|------|------|------|
| YAML | 可读性好，支持注释 | 需要额外库，缩进敏感 |
| JSON | 简单，nlohmann/json header-only | 不支持注释 |
| TOML | 可读性好 | 相对小众 |
| INI | 极简 | 功能有限 |

**理由**: nlohmann/json 是 C++ 社区最流行的 JSON 库，header-only 无需编译，C++17 兼容，API 简洁。

### 2. 配置文件结构：按进程拆分

**选择**: 每个进程一个配置文件

```
config/
├── dbmgr.json
├── gate_server.json
└── game_server.json
```

**备选方案**: 单文件 `servers.json` 管理所有配置

**理由**: 按进程拆分更清晰，部署时可以只修改需要的配置，避免单文件过大。

### 3. 优雅停服：管理消息方式

**选择**: 通过 TCP 连接发送 Shutdown 消息

**备选方案**: 发送 Ctrl+C 信号（taskkill /PID）

**理由**: 消息方式更优雅，可以让服务有序清理资源、保存数据后再退出。

**消息 ID 范围**:
- Gate <-> Game: 3000-3299
- Game <-> DBMgr: 4000-4299
- **管理消息: 5000-5999**（新增）

**停服流程**:
```
stop_graceful.bat
    │
    ▼
gate_server (MSG_ID_SHUTDOWN)
    │
    ├─► game_server_1 (MSG_ID_SHUTDOWN)
    │       │
    │       └─► dbmgr (MSG_ID_SHUTDOWN)
    │               │
    │               └─► SHUTDOWN_RESP ──► game_server ──► gate_server
    │
    └─► gate_server 退出
```

**超时机制**: 60 秒无响应则强制退出

### 4. 编译输出：统一到 bin/

**选择**: 所有可执行文件和 DLL 输出到项目根目录的 `bin/`

**理由**:
- 脚本路径简单，只需引用 `bin\gate_server.exe`
- DLL 共享，三个服务共用 libevent DLL，只需一份
- 部署方便，复制 `bin/` + `config/` 即可

### 5. 启动顺序

**选择**: dbmgr → game_server → gate_server

**理由**: 依赖关系决定启动顺序。gate_server 依赖 game_server，game_server 依赖 dbmgr。

**等待时间**: 每个服务启动后等待 2 秒再启动下一个

### 6. PID 文件管理

**位置**: `runtimeData/` 目录

**格式**: 纯文本，只包含 PID 数字

**写入时机**: 服务启动后立即写入

**读取时机**: 强制停服脚本读取

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|----------|
| 优雅停服超时后仍有服务未退出 | 超时后强制 taskkill |
| JSON 配置文件格式错误导致启动失败 | 启动时验证配置，输出明确错误信息 |
| nlohmann/json 头文件较大（~900KB） | header-only，只在编译时影响，不影响运行时 |
| PID 文件残留（服务异常退出） | 启动时覆盖写入，不依赖旧 PID |
| Windows 平台限制 | 本次仅支持 Windows，后续可扩展 |

## Open Questions

- 是否需要支持多个 dbmgr 实例？（当前配置支持，但脚本可能需要调整）
- 是否需要日志输出到文件？（当前仅 stdout）
