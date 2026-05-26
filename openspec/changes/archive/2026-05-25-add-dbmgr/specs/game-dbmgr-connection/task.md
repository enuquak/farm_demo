# Task: game-dbmgr-connection - Game 侧 DBMgr 连接管理

## 任务拆分

### 1. 构建配置：添加 dbmgr.pb 依赖到 game_server CMakeLists.txt
- [已完成 ✅] 1.1 在 CMakeLists.txt 的 PROTO_SRCS 和 PROTO_HDRS 中添加 dbmgr.pb.cc / dbmgr.pb.h
- [已完成 ✅] 1.2 在 SOURCES 列表中添加新文件占位（后续步骤补充具体文件）

### 2. DBMgr 消息 ID 常量定义
- [已完成 ✅] 2.1 创建 `src/dbmgr_msg_ids.h`，引用 `scripts/server/dbmgr/src/internal_msg_ids.h` 中的常量（或直接 include），避免重复定义

### 3. DBMgrConnection 类实现
- [已完成 ✅] 3.1 创建 `src/dbmgr_connection.h`：定义 DBMgrConnectionState 枚举（CONNECTED, IDENTIFIED, DISCONNECTED）、DBMgrConnection 类
- [已完成 ✅] 3.2 创建 `src/dbmgr_connection.cpp`：实现构造/析构、状态管理、读缓冲区管理、心跳时间更新

### 4. DBMgrConnectionManager 类实现
- [已完成 ✅] 4.1 创建 `src/dbmgr_connection_manager.h`：定义 DBMgrConnectionManager 类（连接管理、请求路由、异步请求队列）
- [已完成 ✅] 4.2 创建 `src/dbmgr_connection_manager.cpp`：实现启动连接、libevent 回调、消息路由、心跳检测、重连逻辑
- [已完成 ✅] 4.3 实现异步请求队列：request_id 生成、pending_requests 映射、callback 注册与匹配
- [已完成 ✅] 4.4 实现请求路由：player_id % dbmgr_count 计算目标 DBMgr
- [已完成 ✅] 4.5 实现 DBMgr 断开时清理 pending requests（调用 callback 返回失败）

### 5. GameServer 集成
- [已完成 ✅] 5.1 在 GameServer 中添加 DBMgrConnectionManager 成员
- [已完成 ✅] 5.2 在 GameServer::start() 中解析 dbmgr_list 配置并初始化 DBMgrConnectionManager
- [已完成 ✅] 5.3 在 GameServer::stop() 中清理 DBMgrConnectionManager
- [已完成 ✅] 5.4 更新 main.cpp 支持 dbmgr 配置参数

### 6. 编译验证与 DLL 检查
- [已完成 ✅] 6.1 执行编译：`cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\game_server"`
- [已完成 ✅] 6.2 验证 Release 目录下 DLL 完整性（event.dll, event_core.dll, event_extra.dll）
- [已完成 ✅] 6.3 编译失败时修复代码直到编译通过（修复了 include 路径和常量命名冲突）
