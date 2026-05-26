# DBMgr 进程开发任务

## 任务列表

### 1. 创建 dbmgr.proto 消息定义 [已完成 ✅]
- 新增 `scripts/common/proto/dbmgr.proto`
- 定义消息: DBMgrIdentify, DBMgrIdentifyResp, DBMgrHeartbeat, DBMgrHeartbeatResp, PlayerDataReq, PlayerDataResp
- MsgID 范围: 4000-4299

### 2. 生成 Protobuf 代码 [已完成 ✅]
- 使用 protoc 生成 C++ 和 Python 绑定
- 输出到 `scripts/common/proto/generated/`

### 3. 创建 DBMgr 项目目录结构 [已完成 ✅]
- 创建 `scripts/server/dbmgr/src/` 目录
- 创建 `scripts/server/dbmgr/CMakeLists.txt`

### 4. 创建 internal_msg_ids.h 消息 ID 常量 [已完成 ✅]
- 定义 DBMgr 相关消息 ID (4001-4006)
- 定义心跳超时等常量

### 5. 创建 message_parser 消息解析器 [已完成 ✅]
- 复用 gate_server 的消息解析模式
- 支持消息打包和解析

### 6. 创建 game_session 连接会话管理 [已完成 ✅]
- 管理与 Game Server 的 TCP 连接
- 状态管理: CONNECTED -> IDENTIFIED -> DISCONNECTED
- 读缓冲区管理（处理 TCP 拆包）

### 7. 创建 data_manager 数据管理器 [已完成 ✅]
- JSON 文件读写操作
- 支持 GET/SET/DEL/GET_ALL/SET_ALL 操作
- 文件路径: data-dir/players/{player_id}.json

### 8. 创建 dbmgr_server 主服务器类 [已完成 ✅]
- TCP 监听，接受 Game 连接
- 心跳定时器（5秒间隔）
- 消息路由: 身份标识、心跳、数据请求
- 连接断开清理

### 9. 创建 main.cpp 入口文件 [已完成 ✅]
- WSAStartup 初始化（Windows）
- 命令行参数解析: --index, --port, --data-dir
- 信号处理: SIGINT, SIGTERM
- 启动服务器

### 10. 编译验证和 DLL 检查 [已完成 ✅]
- 使用 build_cpp14.bat 编译
- 检查 libevent DLL 依赖完整性
- 验证 exe 可运行
