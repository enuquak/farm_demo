---
name: energy-system
description: 能量系统：能量模型、消耗/恢复规则、耗尽处理、同步机制。
metadata:
  type: reference
---

# 能量系统

## 概述

服务器端能量管理系统，负责管理玩家能量数据（当前值/最大值），支持消耗与恢复。每次物品交互消耗对应能量，能量不足时阻止交互并通知客户端显示弹窗。

## 架构设计

### 能量模型

- **初始值**：新玩家首次进入游戏时创建 Energy 实例，`current=100`, `max_energy=100`
- **数据结构**：Energy 类管理当前能量值和最大能量值
- **持久化**：能量数据纳入 DBMgr 持久化，玩家下线时通过 `energy.serialize()` 保存，登录时通过 `Energy.deserialize(data)` 恢复
- **旧存档兼容**：存档中无 energy 字段时默认 `current=100`, `max=100`

### 消耗规则

- 物品使用前 SHALL 检查并扣除能量
- 能量消耗量由 `ITEM_EFFECTS` 配置中的 `energy_cost` 字段决定
- 调用 `energy.consume(amount)` 消耗能量
- 能量充足时：`current` 更新为 `current - amount`，返回 `True`
- 能量不足时：`current` 保持不变，返回 `False`
- 无 `energy_cost` 字段的效果为免费交互，不消耗能量

### 恢复规则

- 食物恢复能量，恢复量由 `ITEM_EFFECTS` 配置中的 `energy_restore` 字段决定
- 调用 `energy.restore(amount)` 恢复能量
- 恢复后 `current` 不超过 `max_energy` 上限

## 关键流程

### 能量消耗流程

1. 客户端发送物品使用请求
2. 服务器检查物品效果配置中的 `energy_cost` 字段
3. 若存在 `energy_cost`，调用 `energy.consume(energy_cost)`
4. 若消耗成功（返回 `True`），执行物品效果
5. 若消耗失败（返回 `False`），返回 `ENERGY_EXHAUSTED` 错误码，不执行效果
6. 在 `ItemUseResp` 中包含 `EnergySync` 字段同步最新能量值

### 食物恢复流程

1. 客户端发送食物使用请求
2. 服务器检查食物效果配置中的 `energy_restore` 字段
3. 调用 `energy.restore(energy_restore)`
4. `current` 更新为 `min(current + energy_restore, max_energy)`
5. 消耗食物物品
6. 在 `ItemUseResp` 中包含 `EnergySync` 字段同步最新能量值

### 耗尽处理

#### 服务器端

- 能量不足导致交互失败时，返回 `ItemUseResp` 且 `code=ENERGY_EXHAUSTED`

#### 客户端

- 收到 `code=ENERGY_EXHAUSTED` 的 `ItemUseResp` 时，显示居中弹窗
- 弹窗内容："精疲力尽！能量不足，无法执行此操作"
- 弹窗包含"确定"按钮
- 弹窗显示期间屏蔽游戏输入（移动、交互均不可用）
- 点击"确定"或按 Enter/Escape 关闭弹窗，恢复游戏输入

### EnergySync 同步

- **登录时同步**：玩家登录成功后，服务器发送 `EnergySync {current, max}`
- **变更时同步**：能量因交互发生变化时，`ItemUseResp` 中包含 `EnergySync` 字段

## 关键代码路径

- **player.cpp**：能量管理核心逻辑，包含 Energy 类实现、consume/restore 方法
- **ui/energy_bar.py**：能量条 UI 组件，右下角显示 40×120 像素的竖条能量条
  - 从下往上填充，显示 "current/max" 数字
  - 颜色根据能量比例变化：
    - 比例 >= 0.8：绿色 (#4caf50)
    - 比例 0.4-0.8：橙黄色 (#ff9800)
    - 比例 < 0.4：红色 (#f44336)
- **ui/exhaustion_modal.py**：精疲力尽弹窗组件

## 常见陷阱

### 能量值未同步

- 问题：能量变化后客户端未及时更新显示
- 原因：`ItemUseResp` 中未包含 `EnergySync` 字段
- 解决：确保每次能量变化后都在响应中包含 `EnergySync`

### 耗尽状态未重置

- 问题：弹窗关闭后游戏输入未恢复
- 原因：弹窗关闭事件未正确处理输入状态
- 解决：确保弹窗关闭时恢复游戏输入

### 恢复超过上限

- 问题：能量恢复后超过最大值
- 原因：restore 方法未正确限制上限
- 解决：恢复时取 `min(current + amount, max_energy)`

## 扩展指南

### 添加新的能量消耗效果

1. 在 `ITEM_EFFECTS` 配置中为新物品添加 `energy_cost` 字段
2. 确保物品使用流程调用 `energy.consume(energy_cost)`
3. 处理消耗失败时返回 `ENERGY_EXHAUSTED` 错误码

### 添加新的能量恢复效果

1. 在 `ITEM_EFFECTS` 配置中为新食物添加 `energy_restore` 字段
2. 确保食物使用流程调用 `energy.restore(energy_restore)`
3. 确保恢复后不超过 `max_energy` 上限

## 相关 Skill

- [[item-interaction]]：物品交互系统，能量消耗/恢复的触发源
