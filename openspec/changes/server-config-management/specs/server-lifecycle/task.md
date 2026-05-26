# Task: server-lifecycle

## 任务拆分

### 1. 创建管理消息协议定义 [已完成 ✅]
- **1.1** 在 `scripts/common/include/` 创建 `admin_msg_ids.h` 定义管理消息 ID (5000-5999) [已完成 ✅]
- **1.2** 定义 MSG_ID_SHUTDOWN (5001) 和 MSG_ID_SHUTDOWN_RESP (5002) [已完成 ✅]
- **1.3** 定义消息结构体 AdminShutdownMsg 和 AdminShutdownResp [已完成 ✅]

### 2. 为 GateServer 添加管理消息处理 [已完成 ✅]
- **2.1** 在 gate_server.h 中添加管理消息处理方法声明 [已完成 ✅]
- **2.2** 在 gate_server.cpp 中实现 handle_admin_message 路由 [已完成 ✅]
- **2.3** 实现 handle_shutdown 处理停服请求（停止接受新连接、转发到 GameServer） [已完成 ✅]
- **2.4** 实现 handle_shutdown_resp 处理停服响应 [已完成 ✅]

### 3. 为 GameServer 添加管理消息处理 [已完成 ✅]
- **3.1** 在 game_server.h 中添加管理消息处理方法声明 [已完成 ✅]
- **3.2** 在 game_server.cpp 中实现 handle_admin_message 路由 [已完成 ✅]
- **3.3** 实现 handle_shutdown 处理停服请求（停止接受新连接、转发到 DBMgr） [已完成 ✅]
- **3.4** 实现 handle_shutdown_resp 处理停服响应 [已完成 ✅]

### 4. 为 DbMgrServer 添加管理消息处理 [已完成 ✅]
- **4.1** 在 dbmgr_server.h 中添加管理消息处理方法声明 [已完成 ✅]
- **4.2** 在 dbmgr_server.cpp 中实现 handle_admin_message 路由 [已完成 ✅]
- **4.3** 实现 handle_shutdown 处理停服请求（保存数据、发送响应、退出） [已完成 ✅]
- **4.4** 实现 handle_shutdown_resp 处理停服响应 [已完成 ✅]

### 5. 创建工具脚本 [已完成 ✅]
- **5.1** 创建 `tools/` 目录 [已完成 ✅]
- **5.2** 创建 `tools/start_all.bat` 按顺序启动服务 [已完成 ✅]
- **5.3** 创建 `tools/stop_graceful.bat` 优雅停服脚本 [已完成 ✅]
- **5.4** 创建 `tools/stop_force.bat` 强制停服脚本 [已完成 ✅]
- **5.5** 创建 `tools/build_all.bat` 编译所有服务 [已完成 ✅]

### 6. 编译验证 [已完成 ✅]
- **6.1** 编译 gate_server 验证代码正确性 [已完成 ✅]
- **6.2** 编译 game_server 验证代码正确性 [已完成 ✅]
- **6.3** 编译 dbmgr 验证代码正确性 [已完成 ✅]
- **6.4** 验证 bin/ 目录输出和 DLL 依赖 [已完成 ✅]

## 依赖关系
- 任务 1 (管理消息协议) 是任务 2,3,4 的前置 ✅
- 任务 2,3,4 可并行开发 ✅
- 任务 5 (工具脚本) 可与任务 2,3,4 并行 ✅
- 任务 6 (编译验证) 是最后一步 ✅

## 完成时间
2026-05-27 02:21:00
