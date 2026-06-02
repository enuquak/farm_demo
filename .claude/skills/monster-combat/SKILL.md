---
name: monster-combat
description: 战斗系统架构、Monster 实体、伤害计算、客户端集成。
metadata:
  type: reference
---

# 战斗系统

## 概述

战斗系统支持玩家与 Monster 实体进行实时战斗。服务端负责伤害计算和状态管理，客户端负责表现和交互。

## 架构设计

### 战斗模型

```
Player                    Monster
├── HP: 100/100          ├── HP: 50/50
├── Attack: 10           ├── Attack: 5
├── Defense: 5           ├── Defense: 2
└── Combat Exp: 0        └── Drop Table: [...]
```

### 伤害公式

```
damage = max(1, attacker.attack - defender.defense + random(-2, 2))
```

## 关键流程

### 攻击流程

1. 客户端发送 AttackReq（monster_id）
2. 服务端验证玩家和怪物状态
3. 计算伤害（考虑攻击、防御、随机因素）
4. 更新怪物 HP
5. 如果怪物死亡：处理掉落、经验
6. 广播 CombatResult 给附近玩家

### 怪物死亡处理

1. 计算掉落物（根据掉落表）
2. 生成 DropItem 实体
3. 分配经验值给攻击者
4. 标记怪物为死亡状态
5. 启动刷新计时器

## 关键代码路径

- 战斗处理：`scripts/server/game_server/src/combat_handler.h/cpp`
- Monster 实体：`scripts/server/game_server/src/monster.h/cpp`
- 客户端战斗：`scripts/client/combat/` 目录
- Protobuf：`scripts/common/proto/combat.proto`

## 常见陷阱

### 伤害计算溢出

伤害值为负数或超过 HP：
- 使用 `max(1, damage)` 保证最低伤害
- 使用 `max(0, hp - damage)` 保证 HP 不为负

### 怪物刷新重叠

多个怪物刷新在同一位置：
- 刷新时检查目标位置是否已有怪物
- 使用 Spawn 点配置避免重叠

### 战斗状态同步延迟

客户端显示的 HP 与服务端不一致：
- 服务端计算后立即广播 CombatResult
- 客户端收到后更新本地状态

## 扩展指南

### 添加新怪物类型

1. 在配置文件中定义怪物属性（HP、攻击、防御、掉落表）
2. 在 MonsterManager 中注册新怪物类型
3. 在场景中配置 Spawn 点

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[scene-system]] — 场景管理（怪物刷新）
- [[item-interaction]] — 物品系统（掉落物）
