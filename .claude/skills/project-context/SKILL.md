---
name: project-context
description: 农场游戏项目的背景知识、架构设计和项目规则。用于在开发过程中保持一致性。
license: MIT
metadata:
  author: farm-demo
  version: "1.0"
---

# 农场游戏项目上下文

## 项目背景

这是一款**类星露谷物语**的多人联机农场游戏。

### 核心玩法参考

- 农场经营（种植、浇水、收获）
- NPC 交互
- 物品/背包系统
- 时间/季节系统
- 多人联机：玩家在同一场景中可以看到彼此

### 技术选型

| 组件 | 技术栈 |
|------|--------|
| 服务器 | C++14 |
| 客户端 | Python + PyOpenGL + PyGame |
| 通信协议 | TCP 长连接 + Protobuf |
| 网络库 | libevent（跨平台） |
| 数据库 | SQLite |

---

## 整体架构设计

### 服务器进程角色

```
┌─────────────────────────────────────────────────────────────────┐
│                        服务器架构                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────┐                                               │
│   │   Client    │                                               │
│   │  (Python)   │                                               │
│   └──────┬──────┘                                               │
│          │ TCP 长连接                                            │
│          ▼                                                      │
│   ┌─────────────┐     TCP      ┌─────────────┐                 │
│   │    Gate     │◄────────────►│    Game     │                 │
│   │   网关服务器  │              │  游戏逻辑服  │                 │
│   └─────────────┘              └─────────────┘                 │
│          │                             │                        │
│          │                             ▼                        │
│          │                     ┌─────────────┐                 │
│          └────────────────────►│ DBManager   │                 │
│                                │ 数据库管理器 │                 │
│                                └──────┬──────┘                 │
│                                       │                        │
│                                       ▼                        │
│                                ┌─────────────┐                 │
│                                │  Database   │                 │
│                                │   SQLite    │                 │
│                                └─────────────┘                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 各角色职责

| 角色 | 职责 | 通信方式 |
|------|------|---------|
| **Gate** | 管理客户端连接、会话维护、消息路由转发 | TCP（与客户端、Game） |
| **Game** | 游戏逻辑处理、场景管理、玩家实体 | TCP（与 Gate） |
| **DBManager** | 数据库操作封装、缓存管理 | TCP（与 Game） |
| **Database** | 数据持久化存储 | 本地连接（与 DBManager） |

### 场景与进程关系

- 不同场景可能在同一个 Game 进程中
- 不同场景也可能分布在不同的 Game 进程
- 玩家切换场景 = 玩家实体从一个 Game 进程移动到另一个 Game 进程
- 客户端与 Gate 保持长连接，不需要断开重连

---

## 项目规则

### 1. Spec 文件命名规则

**规则**: spec 文件夹名称必须包含数字后缀，表示开发顺序。

**格式**: `<capability-name>_<order>`

**示例**:
```
specs/
├── gate-server_1/          ← 第1个开发
│   └── spec.md
├── client-connection_2/    ← 第2个开发（依赖 gate-server）
│   └── spec.md
└── game-logic_3/           ← 第3个开发（依赖前两者）
    └── spec.md
```

**确定顺序的依据**:
- 依赖关系：被依赖的组件先开发
- 基础设施优先：网络层、通信层先于业务逻辑
- 无依赖关系时，按重要性或复杂度排序

### 2. 通信协议规范

**消息格式**:
```
┌──────────────┬──────────────┬──────────────────┐
│ Length (4B)  │ MsgID (4B)   │ Payload (变长)    │
│ 网络字节序   │ 消息类型ID    │ Protobuf 编码    │
└──────────────┴──────────────┴──────────────────┘
```

**心跳机制**:
- 心跳间隔: 5 秒
- 超时断开: 15 秒（3 次心跳未收到）

### 3. Protobuf 消息定义

**基础消息**:
```protobuf
// 心跳
message Heartbeat {
    uint64 timestamp = 1;
}

// 登录请求
message LoginReq {
    string token = 1;
}

// 登录响应
message LoginResp {
    int32 code = 1;
    string msg = 2;
}

// 通用消息封装
message Packet {
    uint32 msg_id = 1;
    bytes payload = 2;
}
```

### 4. 代码组织

**核心规则**: 所有代码相关文件统一放在 `scripts/` 目录下，按 `server/`、`client/`、`common/` 三个子目录分类。

### 5. C++ 编译构建

**构建工具**: 使用 `tool/build_cpp14.bat`（VS2022 + MSVC + C++14）。

**调用方式**:
```bash
cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\<服务目录>"
```

**示例**:
```bash
cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\gate_server"
```

**注意事项**:
- 每个 C++ 服务目录下必须有 `src\main.cpp` 作为编译入口
- 编译成功后在服务目录下生成 `Release/gate_server.exe`

**运行时依赖（关键）**:
- 项目依赖 libevent 动态库，运行时需要以下 DLL 与 exe 同目录：
  - `event.dll`
  - `event_core.dll`
  - `event_extra.dll`
- DLL 来源路径：`C:/libevent_install/lib/`
- build 脚本会在编译成功后自动复制 DLL 到 Release 目录
- protobuf 使用静态链接，无需额外 DLL
- **若 exe 启动报"找不到 xxx.dll"，检查 Release 目录是否包含所有 DLL**

**启动与验证**:
```bash
# 启动服务器（默认端口 8080）
./path/to/gate_server.exe 8080 > server_output.txt 2>&1 &

# 验证端口监听（必须确认后再执行测试）
netstat -ano | grep 8080
```

```
farm_demo/
├── scripts/                    # 所有代码文件
│   ├── server/                # 服务端代码（C++）
│   │   └── gate_server/       # Gate 网关服务器
│   │       ├── src/           # 源码
│   │       ├── proto/         # 服务端专用 proto
│   │       └── CMakeLists.txt
│   ├── client/                # 客户端代码（Python）
│   │   └── ...
│   └── common/                # 公共/共享代码
│       └── proto/             # 共享 Protobuf 定义
│           ├── echo.proto
│           └── generated/     # 生成的代码
├── openspec/                   # OpenSpec 文档
├── agent_workspace_data/       # 智能体工作数据（日志、报告）
├── pre_knowledge/              # 预知识库
│   ├── code/                  # 开发预知识
│   └── test/                  # 测试预知识
└── tmp/                        # 临时文件（测试脚本、临时输出，已 gitignore）
```

**分类原则**:
- `server/` — 服务端专用代码（C++ 服务器、服务端 proto 等）
- `client/` — 客户端专用代码（Python 客户端脚本等）
- `common/` — 跨端共享的代码和资源（proto 定义、生成代码、公共工具等）

---

## 开发阶段规划

### Phase 1: 基础通信（当前）

- [ ] Gate 服务器（TCP 连接管理、心跳、消息解析）
- [ ] 客户端连接模块（TCP 连接、消息收发、心跳）

### Phase 2: 游戏逻辑

- [ ] Game 服务器框架
- [ ] 场景管理
- [ ] 玩家实体同步

### Phase 3: 数据持久化

- [ ] DBManager
- [ ] 玩家数据存储
- [ ] 世界状态保存

### Phase 4: 游戏玩法

- [ ] 农场系统
- [ ] 物品/背包系统
- [ ] NPC 系统
- [ ] 时间/季节系统

---

## 注意事项

1. **跨平台兼容**: 使用 libevent 确保服务器在 Linux/Windows/macOS 都能运行
2. **线程安全**: 客户端使用独立网络线程，需要线程安全队列
3. **协议扩展**: Protobuf 消息设计要考虑后续扩展性
4. **性能考虑**: 心跳包体积小（~20 字节），消息频率适中
