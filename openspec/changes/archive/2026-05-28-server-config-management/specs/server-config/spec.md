## ADDED Requirements

### Requirement: JSON 配置文件格式

系统 SHALL 使用 JSON 格式的配置文件来管理各服务进程的启动参数。

配置文件位置: 项目根目录的 `config/` 文件夹。

#### Scenario: dbmgr 配置文件结构
- **WHEN** 读取 `config/dbmgr.json`
- **THEN** 文件 SHALL 包含以下结构:
  ```json
  {
    "server": {
      "index": 0,
      "ip": "0.0.0.0",
      "port": 5000,
      "data_dir": "./data"
    },
    "database": {
      "uri": "",
      "type": "file"
    },
    "shutdown": {
      "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/dbmgr.pid"
  }
  ```

#### Scenario: gate_server 配置文件结构
- **WHEN** 读取 `config/gate_server.json`
- **THEN** 文件 SHALL 包含以下结构:
  ```json
  {
    "server": {
      "ip": "0.0.0.0",
      "port": 8080
    },
    "game_servers": [
      { "server_id": 1, "ip": "127.0.0.1", "port": 9090 }
    ],
    "shutdown": {
      "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/gate_server.pid"
  }
  ```

#### Scenario: game_server 配置文件结构
- **WHEN** 读取 `config/game_server.json`
- **THEN** 文件 SHALL 包含以下结构:
  ```json
  {
    "server": {
      "ip": "0.0.0.0",
      "port": 9090
    },
    "dbmgrs": [
      { "host": "127.0.0.1", "port": 5000 }
    ],
    "shutdown": {
      "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/game_server.pid"
  }
  ```

### Requirement: 配置文件加载机制

各服务进程启动时 SHALL 从 JSON 配置文件读取启动参数，而非命令行参数。

#### Scenario: 正常加载配置
- **WHEN** 服务启动且配置文件存在且格式正确
- **THEN** 服务 SHALL 使用配置文件中的参数启动

#### Scenario: 配置文件不存在
- **WHEN** 服务启动但配置文件不存在
- **THEN** 服务 SHALL 输出错误信息并退出，错误信息 SHALL 包含缺失的文件路径

#### Scenario: 配置文件格式错误
- **WHEN** 服务启动但配置文件 JSON 格式错误
- **THEN** 服务 SHALL 输出错误信息并退出，错误信息 SHALL 包含解析错误详情

### Requirement: nlohmann/json 库集成

系统 SHALL 使用 nlohmann/json 库进行 JSON 解析。

#### Scenario: 库文件位置
- **WHEN** 查找 JSON 库头文件
- **THEN** 文件 SHALL 位于 `scripts/common/include/nlohmann/json.hpp`

#### Scenario: CMake 集成
- **WHEN** 编译各服务
- **THEN** CMakeLists.txt SHALL 包含 `scripts/common/include` 作为 include 目录

### Requirement: 统一编译输出目录

所有服务的可执行文件和 DLL SHALL 输出到项目根目录的 `bin/` 文件夹。

#### Scenario: 编译输出位置
- **WHEN** 使用 CMake 编译任意服务
- **THEN** 生成的 .exe 文件 SHALL 位于 `bin/` 目录

#### Scenario: DLL 文件共享
- **WHEN** 多个服务依赖相同的 DLL（如 libevent）
- **THEN** DLL 文件 SHALL 在 `bin/` 目录中只保留一份

### Requirement: 配置项说明

#### Scenario: server 配置块
- **WHEN** 解析 `server` 配置块
- **THEN** SHALL 包含以下字段:
  - `ip`: 监听 IP 地址，字符串类型
  - `port`: 监听端口，整数类型
  - `index`: (仅 dbmgr) 实例索引，整数类型
  - `data_dir`: (仅 dbmgr) 数据目录路径，字符串类型

#### Scenario: game_servers 配置块
- **WHEN** 解析 `game_servers` 配置块（gate_server 专用）
- **THEN** SHALL 为数组类型，每个元素包含:
  - `server_id`: 游戏服务器 ID，整数类型
  - `ip`: 游戏服务器 IP，字符串类型
  - `port`: 游戏服务器端口，整数类型

#### Scenario: dbmgrs 配置块
- **WHEN** 解析 `dbmgrs` 配置块（game_server 专用）
- **THEN** SHALL 为数组类型，每个元素包含:
  - `host`: DBMgr 主机地址，字符串类型
  - `port`: DBMgr 端口，整数类型

#### Scenario: database 配置块
- **WHEN** 解析 `database` 配置块
- **THEN** SHALL 包含以下字段:
  - `uri`: 数据库连接 URI，字符串类型
  - `type`: 数据库类型，字符串类型（当前为 "file"）

#### Scenario: shutdown 配置块
- **WHEN** 解析 `shutdown` 配置块
- **THEN** SHALL 包含以下字段:
  - `timeout_ms`: 优雅停服超时时间（毫秒），整数类型

#### Scenario: pid_file 配置项
- **WHEN** 解析 `pid_file` 配置项
- **THEN** SHALL 为字符串类型，指定 PID 文件的输出路径
