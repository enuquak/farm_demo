# 开发任务 - code-1-7a40

## 任务概述
基于 spec 实现 Gate 服务器的核心功能。

## 任务列表

### 1. 定义 Protobuf 消息格式
- [x] 创建 base.proto 定义 Heartbeat, LoginReq, LoginResp, Packet 消息
- [x] 编译生成 C++ 头文件和 Python 绑定

### 2. 实现 Session 管理
- [x] 设计 Session 类结构 (session.h/cpp)
- [x] 实现 SessionManager 管理会话 (session_manager.h/cpp)
- [x] 支持根据 fd 和玩家 ID 查找会话
- [x] 修复: 添加 bind_player_id 方法维护 player_to_fd 映射

### 3. 实现消息解析器
- [x] 设计消息解析器接口 (message_parser.h/cpp)
- [x] 实现长度前缀解析（4字节长度 + 4字节MsgID + Payload）
- [x] 处理粘包：逐个拆分消息
- [x] 处理拆包：缓存不完整数据

### 4. 实现 TCP 连接管理
- [x] 使用 libevent 创建事件循环
- [x] 监听指定端口
- [x] 处理新连接事件
- [x] 处理连接断开事件
- [x] 处理读取事件

### 5. 实现心跳检测
- [x] 创建定时器（每5秒执行）
- [x] 检测超时连接（15秒未心跳）
- [x] 清理超时连接

### 6. 实现消息路由
- [x] 注册消息处理器
- [x] 处理心跳消息
- [x] 处理登录消息
- [x] 预留游戏消息转发接口

### 7. 集成测试
- [x] TC-001: 服务器启动和端口监听
- [x] TC-002: 基本连接和断开
- [x] TC-003: 心跳消息往返
- [x] TC-004: 登录流程
- [x] TC-005: 粘包处理（多条消息一次发送）
- [x] TC-006: 拆包处理（一条消息分多次发送）

## 开发顺序
1. 先定义 Protobuf 消息（任务1）
2. 实现 Session 管理（任务2）
3. 实现消息解析器（任务3）
4. 实现 TCP 连接管理（任务4）
5. 实现心跳检测（任务5）
6. 实现消息路由（任务6）
7. 集成测试（任务7）

## 变更记录
- 修复: SessionManager 添加 bind_player_id 方法，确保登录后 player_to_fd 映射正确维护
- 新增: 集成测试脚本 tests/test_gate_server.py，覆盖 6 个测试用例
- 构建: 项目编译通过 (Release 配置)
- 测试: 6/6 测试全部通过
