## Why

All server processes (gate_server, game_server, dbmgr) and the Python client currently output logs only to the console via raw `std::cout`/`std::cerr` with hardcoded `[Tag]` prefixes. There is no logging framework, no log levels, no file output, and no log rotation. This makes it impossible to diagnose issues after the fact, analyze production behavior, or manage log volume. Adding a unified logging system with file output and rotation is a foundational improvement for operational visibility.

## What Changes

- **引入 spdlog 日志库**（header-only）作为 C++ 服务器的日志基础设施
- **统一日志格式**：`[时间][级别][进程名:PID][模块][内容]`，服务器和客户端保持一致
- **日志文件输出**：服务器写入 `runtimeData/logs/server/`，客户端写入 `runtimeData/logs/client/client_{player_id}.log`
- **日志轮转**：按日轮转 + 单文件超过 20MB 轮转，保留 7 个历史文件
- **4 级日志**：DEBUG、INFO、ERROR、CRITICAL，支持配置文件设置最低输出级别
- **模块注册机制**：在 `scripts/common/include/log_modules.h` 中统一定义所有模块常量（`constexpr string_view`），未来新增模块只需加一行
- **进程标识**：日志中同时输出进程名和 PID（`GetCurrentProcessId()`）
- **Python 客户端**：使用标准 `logging` 模块 + `TimedRotatingFileHandler` + `RotatingFileHandler`，实现相同的格式和轮转策略
- **配置扩展**：各进程 JSON 配置文件新增 `logging` 字段（dir、level、max_file_size_mb、max_files）
- **替换现有日志调用**：将所有 `std::cout << "[Tag]"` 和 `std::cerr << "[Tag]"` 替换为结构化日志调用

## Capabilities

### New Capabilities
- `logging`: 统一日志系统，包括日志库集成、文件输出、轮转策略、模块注册、日志格式定义、配置管理

### Modified Capabilities
<!-- 无需修改现有 spec 的需求 -->

## Impact

- **新增依赖**：spdlog（header-only，通过 vcpkg 或手动引入）
- **CMakeLists.txt**：三个服务器项目的 CMake 配置需要包含 spdlog 头文件路径
- **config_loader.h**：配置结构体新增 `LoggingConfig` 字段
- **JSON 配置文件**：`gate_server.json`、`game_server.json`、`dbmgr.json` 新增 `logging` 配置段
- **所有 C++ 源文件**：替换现有的 `std::cout`/`std::cerr` 日志调用（约 11 个源文件）
- **Python 客户端**：`connection.py` 等文件新增日志初始化和调用
- **tools/start_all.bat**：可能需要调整工作目录确保日志路径正确
- **.gitignore**：需添加 `runtimeData/logs/` 忽略规则
