# Task: server-config

## 任务拆分

### 1. 集成 nlohmann/json 库 [已完成 ✅]
- 1.1 下载 nlohmann/json 单头文件到 `scripts/common/include/nlohmann/json.hpp` [已完成 ✅]
- 1.2 验证头文件可正常包含 [已完成 ✅]

### 2. 创建 JSON 配置文件 [已完成 ✅]
- 2.1 创建 `config/` 目录 [已完成 ✅]
- 2.2 创建 `config/dbmgr.json`（包含 server, database, shutdown, pid_file 配置块） [已完成 ✅]
- 2.3 创建 `config/gate_server.json`（包含 server, game_servers, shutdown, pid_file 配置块） [已完成 ✅]
- 2.4 创建 `config/game_server.json`（包含 server, dbmgrs, shutdown, pid_file 配置块） [已完成 ✅]

### 3. 修改 CMakeLists.txt [已完成 ✅]
- 3.1 修改 dbmgr 的 CMakeLists.txt：添加 nlohmann/json include 目录，修改输出目录到 bin/ [已完成 ✅]
- 3.2 修改 gate_server 的 CMakeLists.txt：添加 nlohmann/json include 目录，修改输出目录到 bin/ [已完成 ✅]
- 3.3 修改 game_server 的 CMakeLists.txt：添加 nlohmann/json include 目录，修改输出目录到 bin/ [已完成 ✅]

### 4. 修改 dbmgr main.cpp [已完成 ✅]
- 4.1 添加 nlohmann/json 头文件包含 [已完成 ✅]
- 4.2 实现配置文件加载函数（读取 config/dbmgr.json） [已完成 ✅]
- 4.3 添加配置文件不存在时的错误处理 [已完成 ✅]
- 4.4 添加 JSON 格式错误时的错误处理 [已完成 ✅]
- 4.5 替换命令行参数解析为 JSON 配置读取 [已完成 ✅]
- 4.6 实现 PID 文件写入功能 [已完成 ✅]

### 5. 修改 gate_server main.cpp [已完成 ✅]
- 5.1 添加 nlohmann/json 头文件包含 [已完成 ✅]
- 5.2 实现配置文件加载函数（读取 config/gate_server.json） [已完成 ✅]
- 5.3 添加配置文件不存在时的错误处理 [已完成 ✅]
- 5.4 添加 JSON 格式错误时的错误处理 [已完成 ✅]
- 5.5 替换命令行参数解析为 JSON 配置读取 [已完成 ✅]
- 5.6 实现 PID 文件写入功能 [已完成 ✅]

### 6. 修改 game_server main.cpp [已完成 ✅]
- 6.1 添加 nlohmann/json 头文件包含 [已完成 ✅]
- 6.2 实现配置文件加载函数（读取 config/game_server.json） [已完成 ✅]
- 6.3 添加配置文件不存在时的错误处理 [已完成 ✅]
- 6.4 添加 JSON 格式错误时的错误处理 [已完成 ✅]
- 6.5 替换命令行参数解析为 JSON 配置读取 [已完成 ✅]
- 6.6 实现 PID 文件写入功能 [已完成 ✅]

### 7. 编译验证 [已完成 ✅]
- 7.1 编译 dbmgr 并验证输出到 bin/ [已完成 ✅]
- 7.2 编译 gate_server 并验证输出到 bin/ [已完成 ✅]
- 7.3 编译 game_server 并验证输出到 bin/ [已完成 ✅]
- 7.4 验证 DLL 文件在 bin/ 目录中 [已完成 ✅]
