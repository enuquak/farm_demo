---
name: global-player-id
description: Player ID 生成策略、PlayerIdGenerator 类、ID 分配流程与容量规划。
metadata:
  type: reference
---

# 全局 Player ID 系统

## 概述

Player ID 使用 `(server_id << 20) | sequence` 格式生成，支持最多 4096 个服务器，每个服务器 1048576 个玩家。ID 全局唯一，由 Game Server 在创建角色时生成。

## 架构设计

### ID 格式

```
[server_id: 12 bit][sequence: 20 bit]
```

- server_id: 0-4095，标识服务器
- sequence: 0-1048575，每个服务器内的自增序列
- 最终 ID = (server_id << 20) | sequence

### PlayerIdGenerator 类

```cpp
class PlayerIdGenerator {
public:
    uint64_t generate(uint32_t server_id);

private:
    std::mutex mutex_;
    std::unordered_map<uint32_t, uint64_t> sequences_;
};
```

- 使用 `std::mutex` 保证线程安全
- 每个 server_id 独立的 sequence 计数器
- 首次调用某个 server_id 时从 1 开始

## 关键流程

### ID 分配流程

1. 客户端发送 CreateRoleReq（包含 server_id、role_name）
2. Game Server 调用 `player_id_generator.generate(server_id)`
3. 生成唯一 player_id
4. 使用 player_id 创建角色数据并保存到 DBMgr
5. 返回 CreateRoleResp（包含 player_id）

### 唯一性保证

- 同一 Game Server 内：mutex 保证 sequence 原子递增
- 不同 Game Server：server_id 不同，高位不同
- 重启后：sequence 从 DBMgr 中已有数据的最大值 +1 开始（TODO: 当前实现未处理）

## 关键代码路径

- ID 生成器：`scripts/server/game_server/src/player_id_generator.h`
- 使用位置：`scripts/server/game_server/src/game_server.cpp`（CreateRoleReq 处理）

## 常见陷阱

### ID 冲突

不同服务器生成相同 ID：
- 确保每个服务器配置不同的 server_id
- server_id 必须在 0-4095 范围内

### sequence 溢出

单个服务器玩家数超过 1048576：
- 当前设计不处理溢出情况
- 需要在运维层面监控玩家数量

### server_id 范围越界

server_id 超过 12 bit 范围：
- 配置校验应在服务启动时执行
- 超出范围会导致 ID 重叠

## 扩展指南

### 扩展 ID 容量

如需支持更多服务器或玩家：
1. 调整 server_id 和 sequence 的 bit 分配
2. 更新 PlayerIdGenerator 的位移常量
3. 确保所有使用 player_id 的代码兼容新格式

## 相关 Skill

- [[server-architecture]] — 服务器架构与配置
- [[player-persistence]] — 玩家数据持久化
