# 玩家实体与移动功能完整性补全设计

**日期**: 2026-06-02
**状态**: 待实现
**方案**: 方案 B — 服务端权威 + 位置校验

---

## 背景

玩家实体和移动功能的客户端逻辑已完整（输入处理、碰撞检测、动画、边界裁剪），但网络同步层存在 4 个缺口导致功能无法端到端工作：

1. Proto 消息 `PositionUpdate`/`PositionCorrect` 未定义，客户端运行时崩溃
2. 服务端未注册 `MSG_ID_POSITION_UPDATE (2101)` handler，位置更新被静默丢弃
3. 位置校正机制代码已写好但因 proto 缺失完全不工作
4. 新玩家默认出生点 (0,0) 与场景出生点不一致

## 目标

- 消除运行时崩溃
- 实现服务端权威的位置校验（边界 + 速度检查）
- 位置校正端到端打通
- 新玩家出生点一致

## 非目标

- 服务端碰撞检测（与地图 tile 碰撞）— 客户端已做，服务端暂不需要
- 反作弊系统 — 农场游戏无 PvP，优先级低
- 多玩家位置广播 — 当前为单人场景，后续再扩展

---

## 设计详情

### 1. Proto 消息定义

在 `scripts/common/proto/player.proto` 中新增：

```protobuf
message PositionUpdate {
    float pos_x = 1;
    float pos_y = 2;
    float pos_z = 3;
    uint64 timestamp = 4;  // 客户端发送时间戳 (ms)
}

message PositionCorrect {
    float pos_x = 1;
    float pos_y = 2;
    float pos_z = 3;
    uint64 timestamp = 4;  // 原始更新的时间戳
}
```

消息 ID（已在 `message_ids.py` 中定义，无需改动）：
- `MSG_ID_POSITION_UPDATE = 2101`
- `MSG_ID_POSITION_CORRECT = 2102`

### 2. 服务端常量

在 `scripts/server/common/include/game_constants.h` 中新增：

```cpp
// 移动校验
constexpr float MAX_PLAYER_SPEED = 128.0f;          // 像素/秒（客户端速度64的2倍容差）
constexpr int64_t POSITION_UPDATE_TIMEOUT_MS = 3000; // 超过3秒未更新视为过期

// 新玩家出生点（像素坐标，对应 farm_main 场景 tile 30,25）
constexpr float DEFAULT_SPAWN_POS_X = 480.0f;  // 30 * TILE_SIZE(16)
constexpr float DEFAULT_SPAWN_POS_Y = 400.0f;  // 25 * TILE_SIZE(16)
```

### 3. 服务端位置处理 Handler

在 `game_server.cpp` 中注册 `MSG_ID_POSITION_UPDATE` handler：

**处理流程：**

```
收到 PositionUpdate
    │
    ├─ 查找 Player → 不存在或状态 != LOADED → 忽略
    │
    ├─ 查找 Scene → 不存在 → 忽略
    │
    ├─ 边界检查：pos_x ∈ [0, 地图像素宽-TILE_SIZE], pos_y 同理
    │   └─ 失败 → 发送 PositionCorrect（合法边界位置）
    │
    ├─ 速度检查：distance / time_delta ≤ MAX_PLAYER_SPEED
    │   └─ 失败 → 发送 PositionCorrect（上次合法位置）
    │
    └─ 通过 → 更新 Player pos_x/pos_y/pos_z, mark_dirty
```

**校正发送：** 通过 `GateSession` 将 `PositionCorrect` 序列化为 `PlayerMsg`（msg_id=2102）发送。

**过期包处理：** 如果 `timestamp` 与服务端当前时间差超过 `POSITION_UPDATE_TIMEOUT_MS`，直接忽略（不校正也不更新）。

### 4. 新玩家出生点修复

修改 `player.cpp` 的 `init_default_data()`：

```cpp
// 修改前
data_.pos_x = 0.0f;
data_.pos_y = 0.0f;

// 修改后
data_.pos_x = DEFAULT_SPAWN_POS_X;  // 480.0f
data_.pos_y = DEFAULT_SPAWN_POS_Y;  // 400.0f
```

与 `GameSceneManager::handle_scene_change_req()` 中 farm 场景出生点 (tile 30,25) 一致。

### 5. 客户端修复

**`player_controller.py` — 发送侧：**
- 修复 `update_position_sending()` 中的 `PositionUpdate` 构建，使用正确的 proto 类
- 新增 `timestamp` 字段：`int(time.time() * 1000)`

**`network_dispatcher.py` — 接收侧：**
- 修复 `MSG_ID_POSITION_CORRECT` handler，使用正确的 `PositionCorrect` proto 解析
- 调用 `PlayerController.start_correction(correct.pos_x, correct.pos_y)`
- 现有 lerp 校正逻辑（0.2 秒线性插值）无需修改

---

## 改动文件清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `scripts/common/proto/player.proto` | 新增 | PositionUpdate, PositionCorrect 消息 |
| `scripts/common/proto/player_pb2.py` | 重新生成 | protoc 编译 |
| `scripts/server/common/include/game_constants.h` | 新增 | MAX_PLAYER_SPEED, POSITION_UPDATE_TIMEOUT_MS, DEFAULT_SPAWN_POS |
| `scripts/server/game_server/src/player.cpp` | 修改 | init_default_data() 出生点 |
| `scripts/server/game_server/src/game_server.cpp` | 新增 | MSG_ID_POSITION_UPDATE handler + 校验逻辑 |
| `scripts/client/player_controller.py` | 修改 | 修复 PositionUpdate 构建 |
| `scripts/client/network_dispatcher.py` | 修改 | 修复 PositionCorrect 解析 |

## 校验规则详细参数

| 参数 | 值 | 说明 |
|------|-----|------|
| MAX_PLAYER_SPEED | 128.0 px/s | 客户端实际速度 64 的 2 倍，容忍网络抖动 |
| POSITION_UPDATE_TIMEOUT_MS | 3000 ms | 超过 3 秒的旧包直接丢弃 |
| 位置变化阈值 | 0.5 px | 客户端发送阈值，避免静止时持续发送 |
| 发送间隔 | 100 ms | 客户端位置上报频率 |
| 校正 lerp 时长 | 200 ms | 客户端平滑校正时间 |

## 测试要点

1. 客户端移动不崩溃（proto 修复）
2. 服务端日志可见收到位置更新
3. 边界外移动被校正回地图内
4. 瞬移（速度超限）被校正回上次合法位置
5. 新玩家出生在 farm_main 场景中心区域
6. 断线重连后位置正确恢复
