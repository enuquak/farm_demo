---
name: game-clock
description: 游戏时钟：时钟模型、日循环、时钟同步、强制睡眠流程。
metadata:
  type: reference
---

## 概述

游戏时钟采用离散时间模型驱动日循环。服务端以 `time_slot` 整数表示当天进度，每个 slot 对应 30 分钟游戏时间，现实 1 分钟对应游戏 30 分钟。时钟管理日/时/分计算、暂停/恢复、slot 事件回调，并通过 ClockSync 消息同步所有客户端。当日循环结束（凌晨 2:00）时自动触发强制睡眠流程。

## 架构设计

### 时钟模型

- **time_slot 范围**：0~39，共 40 个 slot 表示一天
- **每个 slot**：30 游戏分钟，对应现实 60 秒
- **时间映射**：slot 0 = AM 6:00，slot 12 = PM 12:00，slot 36 = AM 0:00
- **一天结束**：slot 达到 40 时触发 `on_day_end` 回调
- **现实与游戏时间比**：1 分钟现实 = 30 分钟游戏

### 日循环

- 每天 40 个 slot，每 slot 60 个 tick
- 帧率无关：通过累积 `dt`（现实秒数）推进，30 FPS 和 60 FPS 在相同现实时间内推进相同的 slot 数
- `elapsed` 字段累积不足一个 slot 的时间，暂停时保留，恢复后继续累积

### 时间显示

提供 `display_time` 属性返回格式化字符串：
- slot 0 → "AM 6:00"
- slot 20 → "PM 4:00"
- slot 38 → "AM 1:00"

## 关键流程

### 时钟同步（ClockSync）

服务器在每个 slot 跳变时广播 `ClockSync` 给所有在线玩家：

1. `time_slot` 从 N 变为 N+1
2. 服务器发送 `ClockSync {day, time_slot, paused}` 给所有客户端
3. 客户端收到后更新本地时钟显示

### 强制睡眠流程

当 `time_slot` 达到 40（AM 2:00）时触发强制睡眠：

1. **触发**：`time_slot` 从 39 变为 40，触发 `on_day_end()` 回调（仅触发一次）
2. **服务端处理**：
   - 立即调用 `clock.pause()` 暂停时钟
   - 发送 `ForceSleepNotify` 给当前玩家
   - 等待客户端回复 `ForceSleepReady`（超时 10 秒强制完成）
3. **客户端过渡动画**：
   - 收到 `ForceSleepNotify` 后启动 Iris 收缩过渡（300ms），屏蔽所有输入
   - Iris 收缩完成后发送 `ForceSleepReady` 给服务器
4. **服务端场景切换**（收到 `ForceSleepReady` 后）：
   - 冻结当前场景，切换到 house 场景，设置玩家位置到 BED 旁边
   - 恢复体力：`energy.restore(energy.max_energy * 0.5)`（恢复 50% 最大体力）
   - 推进天数：`clock.day += 1, clock.time_slot = 0`
   - 调用 `clock.resume()` 恢复时钟
   - 发送 `SceneChangeResp + ClockSync + EnergySync` 给客户端
5. **客户端场景切换**：
   - 收到 `SceneChangeResp` 后替换 TileMap 为 house 场景，更新玩家位置
   - 启动 Iris 展开过渡（300ms）
   - 展开完成后恢复游戏输入，显示新的时间和能量

### 异常处理

- **客户端断线**：服务端仍完成场景切换和天数推进，客户端重连后获取最新状态
- **超时处理**：`ForceSleepNotify` 发送后 10 秒未收到 `ForceSleepReady`，服务端强制完成切换流程

## 关键代码路径

- **game_clock.cpp / game_clock.h** — 时钟核心逻辑（time_slot 推进、暂停/恢复、序列化、回调机制）
- **ui/time_hud.py** — 客户端时钟 HUD 显示

## 常见陷阱

1. **时钟暂停后未恢复**：强制睡眠流程中如果异常中断，`clock.resume()` 可能未被调用，导致时钟永久暂停。需确保在所有分支路径（包括异常）都恢复时钟。
2. **强制睡眠超时**：网络不稳定时客户端可能无法及时回复 `ForceSleepReady`，10 秒超时机制必须正常工作，否则玩家会卡在黑屏状态。
3. **能量恢复计算错误**：恢复的是 50% **最大体力**（`max_energy * 0.5`），不是当前体力的 50%。注意区分 `max_energy` 和 `current_energy`。
4. **on_day_end 重复触发**：slot=40 时必须确保回调只触发一次，否则会导致重复的强制睡眠流程。
5. **elapsed 精度问题**：暂停/恢复时 `elapsed` 保留可能导致累积误差，序列化时注意浮点精度。

## 扩展指南

### 添加季节系统

如需扩展季节系统，建议：
- 在时钟模型中增加 `season` 字段（春/夏/秋/冬）
- 每 N 天（如 28 天）切换季节
- 在 `on_day_end` 回调中检查季节切换逻辑
- `ClockSync` 消息增加 `season` 字段同步给客户端
- 季节影响：作物生长速度、天气概率、NPC 对话等

## 相关 Skill

- [[scene-system]] — 场景切换（强制睡眠中的 house 场景切换依赖此系统）
- [[energy-system]] — 体力系统（强制睡眠恢复 50% 最大体力）
