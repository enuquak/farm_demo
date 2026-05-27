## Context

Farm Demo 客户端当前仅支持 WASD 移动和空格键交互（面前 1 格）。需要扩展输入系统支持鼠标点击交互，并引入 Action Map 抽象层统一管理输入映射。同时需要为所有交互路径添加距离校验。

## Goals / Non-Goals

**Goals:**
- 引入 Action Map 抽象层，将物理按键归并为语义动作
- 支持鼠标位置追踪和按键状态检测
- 鼠标左键点击 tile 可触发物品交互（与空格键等效）
- Camera 支持屏幕坐标→世界坐标转换
- 所有交互路径统一距离校验（切比雪夫距离）
- ITEM_EFFECTS 支持 interactRange 配置

**Non-Goals:**
- 玩家自定义改键位（硬编码即可）
- 鼠标点击时角色转向
- 鼠标悬停 tile 高亮
- 触摸屏/手柄输入

## Decisions

### D1: Action Map 硬编码映射

**选择**: 在 InputManager 中维护硬编码的 Action Map

```python
ACTION_MAP = {
    "move_up":    [K_w, K_UP],
    "move_down":  [K_s, K_DOWN],
    "move_left":  [K_a, K_LEFT],
    "move_right": [K_d, K_RIGHT],
    "interact":   [K_SPACE],
    "open_inventory": [K_e],
    "hotbar_1": [K_1], "hotbar_2": [K_2], ..., "hotbar_0": [K_0],
}
```

**替代方案**: 可配置的键位映射文件。
**理由**: MVP 阶段硬编码足够，后续可轻松升级为配置文件。

### D2: 双入口交互模式

**选择**: 物品交互保留键盘 `try_use(direction)` 不改签名，新增 `try_use_at(tile_x, tile_y)` 支持鼠标指定目标。

```python
# 键盘交互：面前 1 格
def try_use(self, player, direction):
    tx, ty = self._get_facing_tile(player, direction)
    return self._try_use_at(player, tx, ty)

# 鼠标交互：指定 tile
def try_use_at(self, player, tile_x, tile_y):
    return self._try_use_at(player, tile_x, tile_y)
```

**理由**: 双入口更清晰——空格键不需要传坐标，鼠标必须传。不改原有调用点签名。

### D3: 鼠标交互独立监听

**选择**: 鼠标左键在 main.py 中独立监听，不归入 ACTION_MAP 的 interact 动作。

```python
# main.py 事件循环
for event in pygame.event.get():
    if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
        # 屏幕坐标 → 世界 tile 坐标
        world_x, world_y = camera.screen_to_world(*event.pos)
        tile_x = world_x // TILE_SIZE
        tile_y = world_y // TILE_SIZE
        # 检查距离
        if self._in_interaction_range(player, tile_x, tile_y):
            network.send_item_use_at(tile_x, tile_y)
```

**理由**: 空格 interact 的语义是"面前 1 格"，不携带坐标；鼠标左键携带坐标信息。两条路径分别处理更清晰。

### D4: screenToWorld 利用正交投影简化

**选择**: Camera 新增 `screen_to_world(sx, sy)` 方法

```python
def screen_to_world(self, screen_x, screen_y):
    world_x = screen_x + self.x
    world_y = screen_y + self.y
    return world_x, world_y
```

**理由**: 正交投影无缩放/旋转，简单加减即可。

### D5: 距离校验使用切比雪夫距离

**选择**: `distance = max(|player_tile_x - target_x|, |player_tile_y - target_y|)`，与 interactRange 比较。

```python
def _in_interaction_range(self, player, target_x, target_y):
    px = int(player.x // TILE_SIZE)
    py = int(player.y // TILE_SIZE)
    dist = max(abs(px - target_x), abs(py - target_y))
    return dist <= self.current_interact_range
```

**理由**: 切比雪夫距离天然对应九宫格（distance ≤ 1 = 8 邻格 + 自身格），直觉清晰。

### D6: interactRange 默认值策略

**选择**: effect 未声明 interactRange 时默认为 1（九宫格），-1 表示无限制。

**理由**: 绝大多数交互（砍石、翻耕、种植、收割）都是近距离操作，默认 1 最安全。仅食物等自用物品显式声明 -1。

### D7: 鼠标事件屏蔽

**选择**: 当背包面板打开或弹窗显示时，屏蔽鼠标交互事件。

```python
if event.type == pygame.MOUSEBUTTONDOWN:
    if not inventory_panel.is_open and not modal.is_open:
        # 处理鼠标交互
```

**理由**: 避免点击 UI 时误触发游戏交互。

## Risks / Trade-offs

**[Risk] UI 层点击穿透**
→ 鼠标点击在背包面板上时可能误触发交互。
→ Mitigation: 检查 UI 面板状态，打开时屏蔽游戏交互。

**[Risk] 坐标转换精度**
→ PyGame 鼠标坐标是整数，转换到 tile 坐标可能有 1px 偏差。
→ Mitigation: 使用整除运算，天然对齐到 tile 边界。

**[Trade-off] Action Map 硬编码**
→ 当前不支持改键，未来需要可配置时需重构。可接受：先出功能再迭代。
