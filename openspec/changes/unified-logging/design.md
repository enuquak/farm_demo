## Context

当前项目有三个 C++ 服务器进程（gate_server、game_server、dbmgr）和一个 Python 测试客户端。所有日志输出均为原始 `std::cout`/`std::cerr` + 硬编码 `[Tag]` 前缀，无日志框架、无文件输出、无日志级别、无轮转机制。日志仅存在于控制台窗口中，进程关闭后无法回溯。

现有代码中使用了 11 个不同的 `[Tag]` 标签，分布在约 11 个源文件中。配置系统基于 nlohmann/json，已有完整的 JSON 配置加载机制。

## Goals / Non-Goals

**Goals:**
- 所有进程（C++ 服务器 + Python 客户端）日志写入文件，统一输出到 `runtimeData/logs/` 目录
- 统一日志格式：`[时间][级别][进程名:PID][模块][内容]`
- 按日轮转 + 按大小轮转（20MB），保留 7 个历史文件
- 4 个日志级别：DEBUG、INFO、ERROR、CRITICAL
- 模块名在统一位置注册，新增模块只需加一行
- 日志级别可通过配置文件控制

**Non-Goals:**
- 不实现异步日志（项目为单线程事件循环，同步写入足够）
- 不保留控制台输出（纯文件输出）
- 不实现远程日志收集或日志聚合
- 不实现日志按模块过滤（仅按级别过滤）
- 不修改业务逻辑，仅替换日志调用方式

## Decisions

### 1. 日志库选择：spdlog header-only

**选择**: spdlog，header-only 模式引入

**理由**:
- C++ 生态最成熟的日志库，API 简洁，性能优秀
- 原生支持 rotating_file_sink 和 daily_file_sink，满足轮转需求
- header-only 模式无需编译库，降低构建复杂度
- 与 nlohmann/json 同为 header-only，项目风格一致

**替代方案**:
- 自定义 `std::ofstream` 封装：功能有限，需自行实现轮转、格式化、级别过滤，工作量大且容易出错
- glog：需要编译依赖，与项目现有的 header-only 风格不一致

### 2. 双 sink 轮转策略：daily_file_sink + rotating_file_sink

**选择**: 使用 spdlog 的 `daily_file_sink`，结合自定义的大小检查逻辑

**理由**:
- `daily_file_sink` 天然按日期命名文件（`xxx.2026-05-26.log`），满足按日轮转需求
- spdlog 的 `rotating_file_sink` 按大小轮转但不支持日期命名
- 方案：使用 `daily_file_sink` 作为主 sink，在每次写入时检查文件大小，超过 20MB 时手动触发轮转（添加 `.1`, `.2` 后缀）

**替代方案**:
- 仅用 `rotating_file_sink`：无法按日期命名文件
- 仅用 `daily_file_sink`：单日大文件无法分割
- 自定义 sink：开发成本高，daily_file_sink 已满足大部分需求

### 3. 模块注册：constexpr string_view 常量

**选择**: 在 `scripts/common/include/log_modules.h` 中定义 `constexpr std::string_view` 常量

**理由**:
- 避免 `Network = "Network"` 的重复写法
- 编译期确定，零运行时开销
- 新增模块只需加一行常量定义
- C++17 原生支持，与项目标准一致

**替代方案**:
- enum + to_string 映射：需要维护两处（enum 定义和 switch 映射），容易遗漏
- 运行时字符串注册（如 `std::unordered_map`）：增加运行时开销和初始化复杂度

### 4. 进程标识：进程名 + PID

**选择**: 日志中同时输出进程名（从配置/硬编码获取）和 PID（`GetCurrentProcessId()`）

**理由**:
- 进程名提供可读性，PID 提供精确标识
- 同一进程的多个实例可通过 PID 区分
- Windows 使用 `GetCurrentProcessId()`，跨平台可封装

**替代方案**:
- 仅用进程名：无法区分多实例
- 仅用 PID：日志可读性差

### 5. 配置方式：JSON 配置文件 logging 字段

**选择**: 在各进程 JSON 配置文件中新增 `logging` 配置段，在 `config_loader.h` 中新增 `LoggingConfig` 结构体

**理由**:
- 与现有配置体系一致，复用现有的 JSON 解析逻辑
- 每个进程可独立配置日志级别（如生产环境 dbmgr 设为 ERROR，开发环境设为 DEBUG）

**配置结构**:
```json
{
  "logging": {
    "dir": "./runtimeData/logs/server",
    "level": "info",
    "max_file_size_mb": 20,
    "max_files": 7
  }
}
```

### 6. Python 客户端日志方案

**选择**: 使用 Python 标准 `logging` 模块，`TimedRotatingFileHandler` + `RotatingFileHandler`

**理由**:
- Python 标准库，无需额外依赖
- 原生支持按时间和大小轮转
- 通过自定义 Formatter 实现与 C++ 端一致的日志格式

**客户端日志文件命名**: `client_{player_id}.log`，支持多开场景

### 7. spdlog 引入方式

**选择**: 手动下载 spdlog header-only 版本，放入 `scripts/common/third_party/spdlog/`

**理由**:
- 项目目前无包管理器（未使用 vcpkg/conan），手动引入最简单
- 与 nlohmann/json 的引入方式一致（`scripts/common/include/nlohmann/`）
- header-only 无需编译，只需 include 路径

**替代方案**:
- vcpkg：需要引入包管理器，改动较大
- FetchContent：需要 CMake 3.11+，增加构建时间

## Risks / Trade-offs

**[spdlog 版本更新]** → 手动引入的 spdlog 不便自动更新。缓解：锁定版本（如 v1.14.1），后续考虑引入包管理器。

**[daily_file_sink 大小轮转]** → spdlog 的 daily_file_sink 不原生支持按大小轮转，需要自定义逻辑。缓解：封装一个 `DailyRotatingSink`，在 daily_file_sink 基础上增加大小检查。

**[配置加载时机]** → 日志系统需要在配置加载之前就可用（配置加载本身会产生日志）。缓解：配置加载阶段使用默认日志配置（INFO 级别，写入默认目录），配置加载完成后重新初始化日志系统。

**[现有代码改造量]** → 约 11 个源文件需要替换日志调用。缓解：机械式替换，每处改动独立，风险低。

## Migration Plan

1. **引入 spdlog** → 下载 header-only 版本到 `scripts/common/third_party/spdlog/`
2. **创建日志基础设施** → `log_modules.h`、日志初始化函数、配置结构体扩展
3. **更新配置文件** → 各 JSON 配置文件新增 `logging` 段
4. **更新 CMakeLists.txt** → 三个服务器项目添加 spdlog include 路径
5. **逐进程替换日志调用** → gate_server → game_server → dbmgr → client
6. **更新 .gitignore** → 添加 `runtimeData/logs/` 忽略规则

回滚策略：每个步骤独立提交，可逐个 revert。
