# Gate Server 修改任务

## 任务拆分（按开发优先级排序）

### 1. 添加内部消息 ID 常量和内部协议头文件 [已完成 ✅]
- 在 gate_server 中添加 internal_msg_ids.h（与 game_server 相同）
- 添加 internal.pb.h 头文件引用
- 更新 CMakeLists.txt 添加 internal.pb.cc 编译

### 2. 实现 GameConnection 类 (game_connection.h/.cpp) [已完成 ✅]
- Game 服务器连接管理
- 包含 fd、bufferevent、连接状态、gate_id
- 管理连接状态（DISCONNECTED、CONNECTING、IDENTIFIED）
- 读缓冲区管理（处理 TCP 拆包）
- 心跳定时器（5 秒发送间隔）
- 重连定时器（5 秒间隔）

### 3. 修改 GateServer 类添加 Game 连接支持 [已完成 ✅]
- 添加 GameConnection 成员变量
- 添加 connect_to_game() 方法
- 添加 handle_game_read() 方法
- 添加 handle_game_disconnect() 方法
- 添加 send_to_game() 方法
- 添加心跳定时器（5 秒发送间隔）
- 添加重连定时器（5 秒间隔）

### 4. 修改消息路由逻辑 [已完成 ✅]
- 修改 route_message() 方法
- 心跳消息（1001）直接处理
- 登录消息（2001）处理后通知 Game
- 游戏逻辑消息（4000+）转发到 Game
- Game 不可用时静默丢弃

### 5. 实现玩家路由管理 [已完成 ✅]
- 添加 player_to_game 路由表
- 登录成功后发送 PLAYER_JOIN 到 Game
- 收到 PLAYER_JOIN_RESP 后注册路由
- 玩家断开时发送 PLAYER_LEAVE 并注销路由
- Game 断开时清理所有路由

### 6. 实现 Game 消息处理 [已完成 ✅]
- 处理 GATE_IDENTIFY_RESP
- 处理 INTERN_HEARTBEAT_RESP
- 处理 PLAYER_JOIN_RESP
- 处理 GAME_MSG（转发到客户端）

### 7. 编译验证和 DLL 检查 [已完成 ✅]
- 使用 cmake 编译 gate_server
- 检查 Release 目录是否包含 gate_server.exe
- 检查 DLL 完整性

### 8. 知识沉淀 [已完成 ✅]
- 总结 Gate Server 修改经验
- 追加到 `pre_knowledge/code/knowledge.md`
