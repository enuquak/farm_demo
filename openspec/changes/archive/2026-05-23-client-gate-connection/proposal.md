## Why

开发一款类星露谷物语的多人联机农场游戏，需要建立客户端与服务器之间的网络通信基础架构。作为整个游戏服务器系统的入口，Gate 服务器负责管理所有客户端连接，是后续 Game 服务器、DBManager 等组件的通信枢纽。本次先实现客户端连接模块和 Gate 服务器，为后续游戏逻辑开发奠定基础。

## What Changes

- 新增 C++14 Gate 服务器，使用 libevent 实现跨平台 TCP 长连接管理
- 实现 Protobuf 消息的编解码和路由转发
- 实现心跳机制，检测和清理无效连接
- 新增 Python 客户端连接模块，基于 PyOpenGL + PyGame 架构
- 定义基础通信协议：心跳、登录、通用消息封装

## Capabilities

### New Capabilities

- `gate-server_1`: Gate 网关服务器，负责 TCP 连接管理、会话维护、消息解析与路由转发，使用 libevent 实现跨平台网络 IO（优先开发）
- `client-connection_2`: Python 客户端连接模块，负责与 Gate 建立 TCP 长连接、Protobuf 消息收发、心跳维持（依赖 gate-server_1）

### Modified Capabilities

（无，这是全新项目）

## Impact

- **新增代码**: Gate 服务器（C++14）、客户端连接模块（Python）
- **新增依赖**: libevent（服务器端）、protobuf（双方）、pygame + pyopengl（客户端）
- **通信协议**: 定义 Protobuf 消息格式，作为客户端与服务器的通信契约
- **架构影响**: 建立网关模式，为后续 Game 服务器、DBManager 等组件预留扩展接口
