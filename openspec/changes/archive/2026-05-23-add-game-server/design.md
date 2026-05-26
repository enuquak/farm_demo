## Context

Phase 1 已完成：Gate Server（C++/libevent）监听 8080 端口接受客户端 TCP 连接，支持心跳检测和登录流程；Python Client 模块可连接 Gate 完成消息收发。

当前架构只覆盖了 Client ↔ Gate 的通信。Gate 收到游戏逻辑消息时直接 log "Unknown msg_id"，没有任何转发机制。需要打通 Gate ↔ Game 通路，使游戏逻辑消息能到达 Game Server 处理。

现有技术栈：C++17、libevent（事件循环/IO 多路复用）、Protocol Buffers lite（消息序列化）、自定义长度前缀二进制协议（4B Length + 4B MsgID + Payload）。

## Goals / Non-Goals

**Goals:**
- 实现 Game Server 框架，能接受 Gate 连接并处理转发的游戏逻辑消息
- Gate 主动连接 Game，建立内部 TCP 长连接通道
- 定义内部通信协议（连接握手、心跳、玩家生命周期、消息转发）
- 提取共享网络库供 Gate 和 Game 共用
- Gate 能检测 Game 断开并定时重连

**Non-Goals:**
- 水平扩展（多 Game 实例）— 本次假设单 Game
- 具体游戏逻辑（场景、农场、背包等）— 只建框架
- 客户端消息加密
- 客户端断线重连策略
- DBManager / 数据持久化

## Decisions

### D1: 连接方向 — Gate 主动连 Game

**选择**: Gate 作为 TCP client 主动连接 Game（Game 监听端口）。

**备选方案**:
- A: Game 主动连 Gate（Game 注册到 Gate）
- B: 通过中心注册服务发现

**理由**: 单 Game 场景下最简单，Gate 启动后连接本地 `127.0.0.1:9090`。Gate 连接失败时定时重连，不影响 Gate 自身对客户端的服务（消息静默丢弃）。

### D2: 内部协议 — 独立 Proto 文件，复用 Wire 格式

**选择**: 新增 `scripts/common/proto/internal.proto` 定义 Gate↔Game 消息，与 `base.proto`（客户端↔Gate）完全隔离。Wire 格式（长度前缀 + MsgID + Payload）复用，MessageParser 通用。

**MsgID 空间划分**:
```
1000-1999  心跳（客户端↔Gate）
2000-2999  登录/认证（客户端↔Gate）
3000-3099  内部：Gate↔Game 连接管理
3100-3199  内部：玩家生命周期
3200-3299  内部：消息转发
4000+      未来：游戏逻辑
```

**内部消息定义**:

| MsgID | 名称 | 方向 | Proto 字段 |
|-------|------|------|-----------|
| 3001 | INTERN_HEARTBEAT | Gate→Game | timestamp |
| 3002 | INTERN_HEARTBEAT_RESP | Game→Gate | timestamp |
| 3003 | GATE_IDENTIFY | Gate→Game | gate_id, address |
| 3004 | GATE_IDENTIFY_RESP | Game→Gate | code, msg |
| 3101 | PLAYER_JOIN | Gate→Game | player_id |
| 3102 | PLAYER_JOIN_RESP | Game→Gate | player_id, code, msg |
| 3103 | PLAYER_LEAVE | Gate→Game | player_id |
| 3201 | CLIENT_MSG | Gate→Game | player_id, msg_id, payload |
| 3202 | GAME_MSG | Game→Gate | player_id, msg_id, payload |

**理由**: 内外协议分离避免 MsgID 冲突，职责清晰。Wire 格式复用避免重复造轮子。

### D3: 心跳策略 — Gate 发起，复用 5s/15s 模式

**选择**: Gate 作为连接发起方发送心跳（MsgID 3001），Game 回应（MsgID 3002）。参数与客户端↔Gate 一致：5 秒发送间隔，15 秒超时断开。

**备选方案**:
- A: 依赖 TCP 层 keepalive / 连接断开事件
- B: 双向互发心跳

**理由**: 应用层心跳比 TCP keepalive 更可控，超时时间精确。单向心跳（Gate→Game）足够——Gate 需要知道 Game 是否存活以决定能否转发消息；Game 侧通过 TCP 断开事件也能感知 Gate 故障。

### D4: 故障处理 — 静默丢弃 + 定时重连

**选择**: Gate 连接 Game 失败或 Game 断开时：
1. 清理 player_to_game 路由表
2. 启动定时重连（每 5 秒尝试一次）
3. 重连期间客户端游戏消息静默丢弃（不缓存、不报错给客户端）

**理由**: 单 Game 场景下，Game 不可用意味着游戏逻辑完全不可服务，缓存消息没有意义。客户端有自己的重试机制（应用层重发）。重连定时器使用 libevent 的 `evtimer_new` + `EV_PERSIST`。

### D5: 消息转发机制

**选择**: Gate 收到客户端游戏逻辑消息（MsgID 4000+）后：
1. 从 SessionManager 的 player_to_game 表查找目标 Game
2. 将 player_id + 原始 msg_id + 原始 payload 打包为 `ClientMessage`（MsgID 3201）
3. 通过 Game 连接的 bufferevent 发送

Game 返回时反向：
1. 收到 `GameMessage`（MsgID 3202），解包 player_id + msg_id + payload
2. 通过 player_to_fd 表查找客户端 Session
3. 直接用原始 msg_id + payload 发送给客户端（不包装内部协议）

**理由**: 客户端和 Game 之间的业务消息格式保持一致，Gate 只做透明转发，不解析业务 payload。

### D6: 共享库范围 — 最小化提取

**选择**: `scripts/server/common/` 只包含：
- `message_parser.h/.cpp` — 通用的消息解析/打包
- `msvc_compat.cpp` — MSVC ABI 兼容 shim

**不放入 common 的**:
- Session — Gate 和 Game 的 Session 结构差异大（Gate: 客户端连接状态 + player_id；Game: 玩家实体 + 场景状态）
- 事件循环 — 各自用各自的 libevent event_base，结构类似但不需要强制统一

**理由**: 最小化重构，只提取真正通用的部分。避免过度抽象。

### D7: Game Server 架构 — 单线程 libevent 事件循环

**选择**: Game Server 复用与 Gate 相同的单线程 libevent 模式：
- 监听 TCP 9090
- evconnlistener 接受 Gate 连接
- bufferevent 处理读写
- 定时器处理心跳检测

**备选方案**:
- A: 多线程（一个线程 per Gate 连接）
- B: 线程池处理业务逻辑

**理由**: 单 Game + 单 Gate 场景下，单线程足够。libevent 事件循环在 Phase 1 已验证稳定。未来需要时可以演进为多线程。

### D8: 构建系统 — CMake 子目录 + 独立 CMakeLists

**选择**:
```
scripts/server/
├── common/
│   ├── CMakeLists.txt          ← 静态库 target: server_common
│   └── src/
├── gate_server/
│   ├── CMakeLists.txt          ← 链接 server_common
│   └── src/
└── game_server/
    ├── CMakeLists.txt          ← 链接 server_common
    └── src/
```

`tool/build_cpp14.bat` 改为支持参数选择构建目标（gate_server 或 game_server），或构建全部。

## Risks / Trade-offs

**[风险] Gate 重构引入回归** — 提取 common 库需要移动 MessageParser 和 msvc_compat 文件，修改 #include 路径和 CMakeLists。
→ 缓解: 重构后立即运行现有 6 个集成测试（test_gate_server.py），确保零回归。

**[风险] 内部协议 MsgID 未来可能不够用** — 3000-3299 只有 300 个 MsgID。
→ 缓解: 单 Game 场景下足够。需要时可以扩展到更大的范围或重新划分。

**[权衡] 静默丢弃 vs 缓存队列** — Game 不可用时消息直接丢弃，可能丢失玩家操作。
→ 接受: 单 Game 场景下 Game 不可用 = 游戏完全不可服务，缓存没有意义。未来多 Game 时需要重新考虑。

**[权衡] 单线程 Game Server** — 无法利用多核。
→ 接受: 初期负载极低，单线程足够。架构上保留了未来演进为多线程的空间。
