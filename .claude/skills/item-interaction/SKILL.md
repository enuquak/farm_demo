---
name: item-interaction
description: 物品交互系统：物品注册表、交互效果系统、DropItem 实体。
metadata:
  type: reference
---

# 物品交互系统

## 概述

数据驱动的物品交互系统，包含物品注册表、交互效果系统和掉落物实体管理。物品定义集中在注册表中配置，新增物品只需添加配置，无需修改逻辑代码。系统支持物品使用、效果执行、掉落物生成与自动拾取等完整交互流程。

## 架构设计

### 物品注册表

物品定义集中存储在 `item_registry.py` 中，采用静态配置方式管理所有物品属性。系统定义了 7 种物品，分为 4 种类型：

- **RESOURCE（资源）**：木材（ID=1）、石头（ID=2）、作物（ID=7），最大堆叠 99
- **TOOL（工具）**：斧头（ID=3）、锄头（ID=4），最大堆叠 1
- **SEED（种子）**：种子（ID=5），最大堆叠 99
- **FOOD（食物）**：面包（ID=6），最大堆叠 20

每个物品包含名称、类型、最大堆叠数等基础属性，并通过 `get_item_def(item_id)` 函数查询。

### ITEM_EFFECTS 表

物品效果通过 `ITEM_EFFECTS` 配置表定义，采用"物品+目标→效果"的映射结构。效果表支持以下配置项：

- `remove_object`：移除地物（如砍石头）
- `set_ground`：设置地面状态（如翻耕）
- `place_object`：放置地物（如播种）
- `consume_self`：消耗物品自身
- `drops`：掉落物产出配置
- `energy_cost`：能量消耗（默认 0）
- `interact_range`：交互距离（默认 1，即九宫格范围，-1 表示无限制）

效果查询优先级：先匹配地物类型（obj:），再匹配地面类型（gnd:），最后使用 ANY 兜底。

## 关键流程

### 物品使用交互流程

1. **客户端发起请求**：玩家按下空格键或鼠标左键点击 tile，客户端发送 `ItemUseReq`（包含方向或目标坐标）
2. **服务器验证**：检查 activeSlot 是否为空、物品效果是否匹配、能量是否充足
3. **执行效果**：根据 ITEM_EFFECTS 配置执行相应操作（修改地物/地面、增减物品、消耗能量）
4. **返回响应**：返回 `ItemUseResp`（code=0 成功，code=1 无手持物品，code=2 效果不匹配，code=ENERGY_EXHAUSTED 能量不足）
5. **冷却限制**：使用后 300ms 内不能再次使用物品

### 掉落物系统

**DropItem 实体**：
- 在效果产出物品时生成，位置在目标 tile 附近随机偏移
- 客户端渲染物品图标并带有浮动动画（sin 波动，幅度 2px）
- 生命周期 300 秒，超时后服务器移除并通知客户端

**自动拾取机制**：
- 玩家与 DropItem 像素距离 < 48px 时自动拾取
- 背包有空间时，物品加入背包，移除 DropItem，发送 InventorySync 和 DropItemSync(pickup)
- 背包满时 DropItem 保留在地面上

### 特殊交互：收割成熟作物

玩家可空手收割面前的成熟作物（CROP_READY）：
- 前提：面前 tile 地物为 CROP_READY，玩家与地物切比雪夫距离 ≤ 1
- 结果：移除 CROP_READY 地物，地面保持 TILLED，背包获得 2~3 个作物

## 关键代码路径

- **物品注册表**：`item_registry.py` — 物品定义与效果查询
- **效果配置**：`item_effects.cpp/.h` — ITEM_EFFECTS 表实现
- **交互处理**：`item_interaction_handler.cpp/.h` — 请求验证与效果执行
- **掉落物管理**：`drop_item_manager.cpp/.h` — DropItem 实体生命周期管理

## 常见陷阱

1. **物品 ID 不匹配**：客户端与服务端物品 ID 不一致导致效果查询失败，确保使用注册表定义的 ID
2. **效果未触发**：检查效果查询优先级，确保 obj/gnd 类型匹配正确，注意 ANY 兜底逻辑
3. **DropItem 内存泄漏**：300 秒生命周期到期后必须清理，避免实体堆积
4. **冷却时间误判**：300ms 冷却期内输入被忽略，但服务器仍会收到请求
5. **背包满时拾取失败**：背包满时 DropItem 不会被拾取，需要玩家清理背包

## 扩展指南

### 添加新物品

1. 在 `item_registry.py` 的物品列表中添加新条目，指定 ID、名称、类型、最大堆叠数
2. 在 `ITEM_EFFECTS` 表中配置该物品的效果映射
3. 确保客户端与服务端物品 ID 同步更新

### 添加新效果

1. 在 `ITEM_EFFECTS` 表中为现有物品添加新的目标类型映射
2. 配置效果参数（如 energy_cost、interact_range、drops 等）
3. 在 `item_effects.cpp/.h` 中实现新的效果处理逻辑（如需要）
4. 更新 `item_interaction_handler.cpp/.h` 中的验证与执行逻辑

## 相关 Skill

- [[scene-system]] — 场景系统，管理地物与地面状态
- [[energy-system]] — 能量系统，处理物品使用的能量消耗