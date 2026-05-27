## ADDED Requirements

### Requirement: 日志文件输出
所有进程（C++ 服务器和 Python 客户端）SHALL 将日志写入文件。服务器日志 SHALL 写入 `runtimeData/logs/server/` 目录，客户端日志 SHALL 写入 `runtimeData/logs/client/` 目录。目录不存在时 SHALL 自动创建。

#### Scenario: 服务器进程启动时创建日志文件
- **WHEN** gate_server 进程启动并初始化日志系统
- **THEN** 在 `runtimeData/logs/server/` 目录下创建 `gate_server.log` 文件

#### Scenario: 客户端启动时创建日志文件
- **WHEN** Python 客户端以 player_id="alice" 启动并初始化日志系统
- **THEN** 在 `runtimeData/logs/client/` 目录下创建 `client_alice.log` 文件

#### Scenario: 日志目录不存在时自动创建
- **WHEN** 进程首次启动，`runtimeData/logs/server/` 目录不存在
- **THEN** 系统 SHALL 自动创建该目录及其父目录

### Requirement: 统一日志格式
所有进程的日志输出 SHALL 使用统一格式：`[时间][级别][进程名:PID][模块][内容]`。时间格式 SHALL 为 `YYYY-MM-DD HH:MM:SS.mmm`。级别 SHALL 为右对齐的大写英文（DEBUG/INFO /ERROR/CRITICAL）。

#### Scenario: 标准日志格式
- **WHEN** gate_server 输出一条 INFO 级别日志，模块为 Gate，内容为 "新玩家连接: 192.168.1.100"，进程 PID 为 12345
- **THEN** 日志文件中写入：`[2026-05-26 14:30:05.123][INFO ][gate_server:12345][Gate      ] 新玩家连接: 192.168.1.100`

#### Scenario: ERROR 级别日志格式
- **WHEN** dbmgr 输出一条 ERROR 级别日志，模块为 DBMgr，内容为 "查询超时: player_data"，进程 PID 为 12346
- **THEN** 日志文件中写入：`[2026-05-26 14:30:06.456][ERROR][dbmgr:12346     ][DBMgr     ] 查询超时: player_data`

#### Scenario: 客户端日志格式与服务器一致
- **WHEN** Python 客户端输出一条 INFO 级别日志，模块为 Inventory，内容为 "背包已满"
- **THEN** 日志文件中使用相同的格式规范：`[时间][级别][client:PID][模块][内容]`

### Requirement: 日志级别
系统 SHALL 支持 4 个日志级别：DEBUG、INFO、ERROR、CRITICAL。日志级别 SHALL 可通过配置文件设置最低输出级别，低于该级别的日志 SHALL 不写入文件。

#### Scenario: 配置为 INFO 级别时过滤 DEBUG 日志
- **WHEN** 配置文件中 `logging.level` 设置为 "info"
- **THEN** DEBUG 级别的日志 SHALL 不写入文件，INFO、ERROR、CRITICAL 级别的日志 SHALL 正常写入

#### Scenario: 配置为 DEBUG 级别时输出所有日志
- **WHEN** 配置文件中 `logging.level` 设置为 "debug"
- **THEN** 所有 4 个级别的日志 SHALL 都写入文件

#### Scenario: 配置为 ERROR 级别时仅输出错误
- **WHEN** 配置文件中 `logging.level` 设置为 "error"
- **THEN** 仅 ERROR 和 CRITICAL 级别的日志写入文件

### Requirement: 日志轮转
系统 SHALL 支持按日轮转和按大小轮转两种策略同时生效。按日轮转 SHALL 在每天 00:00 自动创建新日志文件，文件名包含日期后缀。按大小轮转 SHALL 在单个日志文件超过 20MB 时创建新的轮转文件。系统 SHALL 保留最近 7 个历史文件。

#### Scenario: 按日轮转
- **WHEN** 日期从 2026-05-26 变为 2026-05-27
- **THEN** 当前日志文件 `gate_server.log` 的内容 SHALL 被归档为 `gate_server.2026-05-26.log`，新日志写入 `gate_server.log`

#### Scenario: 按大小轮转
- **WHEN** 当天日志文件 `gate_server.2026-05-26.log` 大小超过 20MB
- **THEN** 系统 SHALL 创建 `gate_server.2026-05-26.log.1`，新日志继续写入新文件

#### Scenario: 历史文件清理
- **WHEN** 轮转后历史文件数量超过 7 个
- **THEN** 系统 SHALL 删除最旧的历史文件，保持总数不超过 7 个

#### Scenario: 客户端日志轮转
- **WHEN** 客户端日志文件 `client_alice.log` 超过 20MB 或日期变更
- **THEN** 客户端 SHALL 执行与服务器相同的轮转策略

### Requirement: 模块注册
系统 SHALL 在 `log_modules.h`（C++）和 `log_modules.py`（Python）中统一定义所有日志模块常量。模块常量 SHALL 使用 `constexpr std::string_view`（C++）或字符串常量（Python）定义。新增模块 SHALL 只需在模块定义文件中添加一行常量。

#### Scenario: 使用已注册模块输出日志
- **WHEN** 代码中使用 `LogModule::Gate` 模块输出日志
- **THEN** 日志文件中模块字段显示为 "Gate"

#### Scenario: 新增模块
- **WHEN** 开发者需要添加排行榜模块的日志
- **THEN** 在 `log_modules.h` 中添加 `constexpr std::string_view Leaderboard = "Leaderboard";`，在 `log_modules.py` 中添加 `Leaderboard = "Leaderboard"`，即可在代码中使用

### Requirement: 进程标识
日志中的进程标识 SHALL 包含进程名和 PID 两部分，格式为 `进程名:PID`。进程名 SHALL 从配置或代码中获取。PID SHALL 通过操作系统 API 获取（Windows: `GetCurrentProcessId()`，POSIX: `getpid()`）。

#### Scenario: 服务器进程标识
- **WHEN** gate_server 进程（PID=12345）输出日志
- **THEN** 日志中进程标识字段为 `gate_server:12345`

#### Scenario: 客户端进程标识
- **WHEN** Python 客户端进程（PID=12347）输出日志
- **THEN** 日志中进程标识字段为 `client:12347`

### Requirement: 日志配置
每个进程的 JSON 配置文件 SHALL 支持 `logging` 配置段，包含 `dir`（日志目录）、`level`（最低日志级别）、`max_file_size_mb`（单文件最大大小）、`max_files`（历史文件保留数量）四个字段。配置缺失时 SHALL 使用默认值。

#### Scenario: 完整日志配置
- **WHEN** 配置文件包含 `logging` 段：`{"dir": "./runtimeData/logs/server", "level": "info", "max_file_size_mb": 20, "max_files": 7}`
- **THEN** 日志系统 SHALL 按配置初始化，输出到指定目录，INFO 级别以上，20MB 轮转，保留 7 个文件

#### Scenario: 配置缺失时使用默认值
- **WHEN** 配置文件中不包含 `logging` 字段
- **THEN** 日志系统 SHALL 使用默认值：dir="./runtimeData/logs/server"，level="info"，max_file_size_mb=20，max_files=7

#### Scenario: 客户端日志配置
- **WHEN** Python 客户端启动时传入日志配置
- **THEN** 客户端 SHALL 按配置初始化日志系统，输出到 `runtimeData/logs/client/` 目录

### Requirement: spdlog 集成
C++ 服务器 SHALL 使用 spdlog header-only 库作为日志基础设施。spdlog SHALL 以 header-only 模式引入，放置在 `scripts/common/third_party/spdlog/` 目录下。CMakeLists.txt SHALL 包含 spdlog 的 include 路径。

#### Scenario: spdlog 头文件引入
- **WHEN** 开发者在 C++ 源文件中 `#include "spdlog/spdlog.h"`
- **THEN** 编译 SHALL 成功，无需链接额外的库文件

#### Scenario: CMake 配置
- **WHEN** 构建 gate_server 项目
- **THEN** CMakeLists.txt SHALL 包含 spdlog 的 include 目录路径
