---
name: crop-system
description: 作物系统：作物生命周期、生长定时器、冻结/解冻支持。
metadata:
  type: reference
---

# 作物系统 (Crop System)

## 概述

作物系统是服务器端的作物生命周期管理模块。它负责管理作物从播种到成熟的完整生命周期，包括：
- 作物状态跟踪（TILLED → CROP_GROWING → CROP_READY）
- 60秒实时生长计时
- 场景冻结/恢复时的生长补帧
- 序列化与反序列化支持

## 架构设计

### 状态机

作物系统采用三态状态机模型：

```
TILLED → CROP_GROWING → CROP_READY
  ↑           ↓              ↓
  └───────────┴──────────────┘
         收割后回到 TILLED
```

- **TILLED**: 翻耕状态，可接受播种
- **CROP_GROWING**: 作物生长中，等待成熟
- **CROP_READY**: 作物已成熟，可收割

### 生长计时机制

- **生长周期**: 60秒（实时）
- **驱动方式**: 服务器定时器驱动
- **数据结构**: 每个生长中的作物记录 `{planted_at: timestamp, grow_time: 60}`
- **成熟判断**: `now - planted_at >= grow_time`

## 关键流程

### 播种流程

1. 玩家使用种子道具
2. 检查目标 tile 地面状态是否为 TILLED
3. 如果是 TILLED：
   - 在目标位置放置 CROP_GROWING 地物
   - CropSystem 注册该 tile 的生长数据：`{planted_at: 当前时间戳, grow_time: 60}`
4. 如果不是 TILLED：
   - 效果不触发，返回错误

### 生长流程

1. CropSystem.update() 被调用（每帧或定时）
2. 遍历所有 growing_tiles
3. 对每个作物检查：`now - planted_at >= grow_time`
4. 如果已成熟：
   - 将该位置的地物从 CROP_GROWING 更新为 CROP_READY
   - 从 growing_tiles 中移除该记录
5. 如果未成熟：
   - 保持 CROP_GROWING 状态不变

### 收获流程

1. 玩家收割 CROP_READY 地物
2. 移除地物
3. CropSystem 移除该 tile 的数据（如果存在）

### 冻结/解冻支持

与 scene-system 集成，支持场景冻结时暂停生长：

**冻结场景**:
1. 玩家离开当前场景
2. 记录 frozen_at 时间戳
3. CropSystem 暂停更新

**恢复场景**:
1. 玩家回到已冻结的场景
2. 计算 elapsed = now - frozen_at
3. 调用 `crop_system.simulate_elapsed(elapsed)` 补帧
4. 补帧期间，所有已成熟的作物立即变为 CROP_READY

**补帧示例**:
- 场景冻结了 120 秒
- 某作物在冻结前 30 秒播种
- 补帧时总经过时间 = 30 + 120 = 150 秒 > 60 秒
- 该作物立即成熟

## 关键代码路径

- `crop_system.cpp/.h` — 作物系统核心实现
- `scene_state.cpp/.h` — 场景状态管理（包含冻结/恢复逻辑）

## 常见陷阱

### 作物状态未同步

**问题**: 地物状态与 CropSystem 内部数据不一致

**场景**:
- CROP_GROWING 地物被非正常方式移除（如GM命令、其他系统干预）
- CropSystem 仍保留该 tile 的生长数据

**解决**: CropSystem 在每次 update 时清理无效数据（检查 growing_tiles 中的 tile 是否仍有对应的 CROP_GROWING 地物）

### 冻结后生长计算错误

**问题**: 场景恢复后作物生长状态不正确

**场景**:
- 冻结时间超过作物成熟时间
- 补帧计算错误导致作物状态异常

**解决**: 使用 `simulate_elapsed(elapsed)` 统一处理补帧，确保所有作物按正确顺序成熟

## 扩展指南

### 添加新作物类型

当前系统使用统一的 60 秒生长周期。如需支持多种作物类型：

1. 修改生长数据结构，添加作物类型字段：
   ```cpp
   struct CropData {
       uint64_t planted_at;
       uint64_t grow_time;  // 可根据作物类型变化
       std::string crop_type;  // 新增
   };
   ```

2. 在播种时根据种子类型设置对应的 grow_time

3. 修改成熟逻辑，支持不同成熟时间

4. 更新序列化/反序列化逻辑以支持新字段

## 相关 Skill

- [[scene-system]] — 场景系统，提供冻结/恢复支持
- [[item-interaction]] — 物品交互系统，处理种子使用逻辑
