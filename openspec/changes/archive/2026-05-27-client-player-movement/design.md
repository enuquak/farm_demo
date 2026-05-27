## Context

Farm Demo 已有 PyGame 登录界面和 TCP 连接能力，即将实现场景网格渲染。需要在此基础上实现玩家角色的键盘控制移动，以及客户端-服务器的位置同步。

当前架构：Python 客户端（PyGame）↔ C++ 服务器（Game Server）。客户端发送操作指令，服务器验证并广播状态。

## Goals / Non-Goals

**Goals:**
- WASD 键盘控制角色自由像素级移动
- 角色四向朝向（上下左右），移动时自动转向
- 客户端预测：按键后立即本地移动，发包给服务器
- 服务器权威：服务器验证位置合法性，异常时纠正客户端
- 相机跟随玩家

**Non-Goals:**
- 不实现角色行走动画（帧切换）
- 不实现碰撞检测（仅地图边界钳位）
- 不实现其他玩家的实时位置显示（后续迭代）
- 不实现冲刺/跑步（固定速度）
- 不实现鼠标点击移动（寻路）

## Decisions

### D1: 移动模型 — 速度向量 + 边界钳位

**选择**: 每帧根据按键状态生成方向向量，归一化后乘以速度常量

```python
# 输入处理
dx, dy = 0, 0
if keys[K_w]: dy -= 1
if keys[K_s]: dy += 1
if keys[K_a]: dx -= 1
if keys[K_d]: dx += 1

if dx != 0 or dy != 0:
    length = math.sqrt(dx*dx + dy*dy)
    dx /= length; dy /= length  # 归一化，防止对角线 √2 倍速

player.x += dx * SPEED * dt
player.y += dy * SPEED * dt

# 边界钳位
player.x = clamp(player.x, 0, map_width - player_width)
player.y = clamp(player.y, 0, map_height - player_height)
```

**理由**: 归一化保证各方向等速。使用 deltaTime 帧率无关。边界钳位是最简碰撞方案。

### D2: 客户端预测 + 服务器验证

**选择**: 客户端先移动再发包，服务器异步验证

```
客户端每帧:
  1. 处理输入 → 本地移动（立即生效）
  2. 每 100ms 发送一次 PositionUpdate 消息（包含 x, y, direction）

服务器收到 PositionUpdate:
  1. 验证位移距离是否合理（speed * elapsed * 1.1 容差）
  2. 验证目标位置是否可通行
  3. 合法 → 更新服务端 Player 数据
  4. 不合法 → 发送 PositionCorrect 消息（包含正确的 x, y）

客户端收到 PositionCorrect:
  1. 平滑插值到服务器指定位置（200ms lerp）
```

**替代方案**: 服务器先行模式（客户端发指令，服务器计算后返回位置）。
**理由**: 客户端预测减少延迟感，对于本地局域网游戏体验更好。服务器验证保证权威性。

### D3: 朝向状态管理

**选择**: Player 维护 `direction` 字段（'up'/'down'/'left'/'right'），移动时自动更新

```python
# 朝向更新优先级：最后输入的非零分量决定朝向
if dx > 0: direction = 'right'
elif dx < 0: direction = 'left'
elif dy > 0: direction = 'down'
elif dy < 0: direction = 'up'
```

**理由**: 简单直观，与后续物品交互（面前 1 格）的方向计算一致。

### D4: 玩家渲染

**选择**: PyGame Surface 绘制角色精灵（纯色占位）

```python
# 角色占 1 格（32×32 像素）
body_rect = pygame.Rect(0, 0, TILE_SIZE, TILE_SIZE)
pygame.draw.rect(surface, PLAYER_COLOR, body_rect)
# 朝向指示：小三角箭头
```

**理由**: 纯色占位，后续替换为精灵图。朝向箭头提供视觉反馈。

### D5: 消息协议

```protobuf
// 客户端 → 服务器
message PositionUpdate {
    float x = 1;
    float y = 2;
    string direction = 3;
    uint64 timestamp = 4;
}

// 服务器 → 客户端（异常纠正）
message PositionCorrect {
    float x = 1;
    float y = 2;
    string direction = 3;
}
```

**同步频率**: 100ms（10 次/秒），局域网足够且不造成过大网络负载。

### D6: 模块结构

```
scripts/client/
├── player.py           # Player 实体类（位置、朝向、渲染）
├── movement_controller.py  # 输入处理 + 移动逻辑 + 发包
```

**职责划分**:
- `player.py` — 纯数据 + 渲染，不依赖网络
- `movement_controller.py` — 输入处理、移动计算、位置同步

## Risks / Trade-offs

**[Risk] 客户端预测导致位置不一致**
→ 服务器验证失败时需要纠正，可能导致角色"瞬移"。
→ Mitigation: 使用 lerp 平滑纠正，200ms 内完成。

**[Risk] 网络延迟导致输入卡顿**
→ 本地网络延迟通常 <1ms，无感知。互联网场景需要额外优化。
→ Mitigation: 当前仅支持局域网，可接受。

**[Trade-off] 同步频率 100ms**
→ 10 次/秒对局域网足够。更高频率增加网络负载但改善同步精度。
→ 可根据实际体验调整。

## Open Questions

- [ ] 是否需要支持其他玩家的实时位置显示？（需要额外的广播协议）
- [ ] 角色尺寸是占满一格（32×32）还是更小（如 24×32）？
