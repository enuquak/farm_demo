# Game Server 开发任务

## 任务拆分（按开发优先级排序）

### 1. 创建 game_server 目录结构和 CMakeLists.txt [已完成 ✅]
- 创建 `scripts/server/game_server/` 目录
- 创建 `scripts/server/game_server/src/` 子目录
- 创建 `scripts/server/game_server/CMakeLists.txt`（参考 gate_server 的 CMakeLists.txt）
- 链接 internal.pb.cc 和 base.pb.cc 的生成代码
- 链接 libevent、protobuf-lite、absl 库

### 2. 定义内部消息 ID 常量 (internal_msg_ids.h) [已完成 ✅]
- 定义 MsgID 常量（3001-3202 范围）
- 包含: INTERN_HEARTBEAT, INTERN_HEARTBEAT_RESP, GATE_IDENTIFY, GATE_IDENTIFY_RESP, PLAYER_JOIN, PLAYER_JOIN_RESP, PLAYER_LEAVE, CLIENT_MSG, GAME_MSG

### 3. 实现 GateSession 类 (gate_session.h/.cpp) [已完成 ✅]
- Gate 连接会话管理
- 包含 fd、bufferevent、gate_id、连接时间、最后心跳时间
- 管理会话状态（CONNECTED、IDENTIFIED、DISCONNECTED）
- 读缓冲区管理（处理 TCP 拆包）

### 4. 实现 Player 类 (player.h/.cpp) [已完成 ✅]
- 玩家对象
- 包含 player_id、关联的 GateSession
- 玩家状态管理

### 5. 实现 PlayerManager 类 (player_manager.h/.cpp) [已完成 ✅]
- 在线玩家管理容器
- 支持 add_player、remove_player、get_player 操作
- 维护 player_id 到 Player 的映射
- 维护 player_id 到 gate_session 的映射

### 6. 实现 MessageHandler 消息处理框架 (message_handler.h/.cpp) [已完成 ✅]
- 支持 register_handler(msg_id, callback) 注册消息处理函数
- 支持 dispatch(msg_id, player_id, payload) 分发消息
- 未注册消息记录警告日志

### 7. 实现 GameServer 主类 (game_server.h/.cpp) [已完成 ✅]
- TCP 监听端口 9090
- libevent 事件循环（单线程）
- 接受 Gate 连接，创建 GateSession
- 心跳检测定时器（15 秒超时）
- Gate 身份识别处理（GATE_IDENTIFY / GATE_IDENTIFY_RESP）
- 内部心跳处理（INTERN_HEARTBEAT / INTERN_HEARTBEAT_RESP）
- 玩家生命周期处理（PLAYER_JOIN / PLAYER_JOIN_RESP / PLAYER_LEAVE）
- 客户端消息分发（CLIENT_MSG → handler → GAME_MSG）
- Gate 连接断开清理

### 8. 创建 main.cpp 入口文件 [已完成 ✅]
- 命令行参数解析（端口、IP）
- 信号处理（SIGINT、SIGTERM）
- Winsock 初始化（Windows）
- 创建并启动 GameServer

### 9. 编译验证和 DLL 检查 [已完成 ✅]
- 使用 `tool/build_cpp14.bat` 编译 game_server
- 检查 Release 目录是否包含 game_server.exe
- 检查 DLL 完整性（event.dll、event_core.dll、event_extra.dll）
- 如 DLL 缺失，更新 build 脚本添加 game_server 的 DLL 复制逻辑

### 10. 知识沉淀 [已完成 ✅]
- 总结 Game Server 开发经验
- 追加到 `pre_knowledge/code/knowledge.md`
