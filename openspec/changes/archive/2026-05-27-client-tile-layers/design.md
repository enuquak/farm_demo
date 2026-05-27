## Context

Farm Demo 的场景网格系统初始实现为单层 TileMap，每个 tile 为纯色方块。石头、作物等物体与地面混为一体，视觉不自然。需要拆分为地面层（Ground）+ 地物层（Object）双层结构，地物使用像素精灵渲染。

## Goals / Non-Goals

**Goals:**
- 地图拆为 ground + objects 双层数据
- 地物（石头/作物）使用像素精灵渲染，带透明通道
- 交互系统适配双层（优先匹配 object，无 object 时匹配 ground）
- 存档格式适配 + 旧存档自动迁移

**Non-Goals:**
- 不实现地面层自动过渡（auto-tiling）
- 不实现地物占据多格（如 1×2 树木）
- 不实现动画精灵（作物生长动画）
- 不实现地面层像素精灵化（地面保持纯色）

## Decisions

### D1: 双层数据结构

**选择**: TileMap 内部维护两个独立二维数组 `_ground[y][x]` (int) 和 `_objects[y][x]` (int | None)。None 表示该格无地物。

```python
class TileMap:
    def __init__(self, width, height):
        self._ground = [[0]*width for _ in range(height)]
        self._objects = [[None]*width for _ in range(height)]
```

**替代方案**: 单数组中每个元素存 dict `{ground, object}`。
**理由**: 两个平坦数组内存紧凑、序列化简单、渲染遍历高效。

### D2: 地物精灵尺寸 16×16，渲染拉伸到 32×32

**选择**: 地物像素数据用 16×16 定义，渲染时通过 PyGame transform.scale 拉伸到 TILE_SIZE。NEAREST 采样保持像素锐利。

**理由**: 与物品图标 8×8→32×32 的放大模式一致，保持像素风。16×16 精度足够表现石头/作物细节。

### D3: 地物类型定义

```python
class ObjectType:
    NONE = 0
    STONE = 1       # 石头
    CROP_GROWING = 2  # 种植中
    CROP_READY = 3    # 成熟作物
    TREE = 4          # 树木
    # 场景切换相关
    DOOR_IN = 10      # 室内门
    DOOR_OUT = 11     # 室外门
    BED = 12          # 床
    TV = 13           # 电视
    STOVE = 14        # 炉子
```

每种 Object 关联：16×16 像素精灵 palette、是否可通行、是否可交互、交互类型。

### D4: 分层渲染

**选择**: 渲染分两遍——先地面后地物

```python
def render(self, surface, camera):
    # 第一遍：地面层（纯色 + 2.5D 高光/阴影）
    for ty in visible_rows:
        for tx in visible_cols:
            ground_id = self._ground[ty][tx]
            color = GROUND_COLORS[ground_id]
            rect = pygame.Rect(tx*TILE_SIZE - cam_x, ty*TILE_SIZE - cam_y, TILE_SIZE, TILE_SIZE)
            pygame.draw.rect(surface, color, rect)
            # 2.5D 高光/阴影
            self._draw_2d5_effect(surface, rect, ground_id)

    # 第二遍：地物层（像素精灵，带透明通道）
    for ty in visible_rows:
        for tx in visible_cols:
            obj_id = self._objects[ty][tx]
            if obj_id is not None:
                sprite = OBJECT_SPRITES[obj_id]
                pos = (tx*TILE_SIZE - cam_x, ty*TILE_SIZE - cam_y)
                surface.blit(sprite, pos)
```

**理由**: 两遍渲染保证地物覆盖在地面之上，透明通道让地面透出来。

### D5: 交互效果类型扩展

**选择**: ITEM_EFFECTS 的效果对象新增三个操作：
- `removeObject: True` — 移除目标格的地物
- `placeObject: objectId` — 在目标格放置地物
- `setGround: groundId` — 修改目标格的地面类型

原有的 `newTile` 废弃，由上述三个操作组合替代。

### D6: getItemEffect 双层匹配策略

**选择**:
1. 如果目标格有 object → 用 `obj:<objectId>` 作为匹配键
2. 如果目标格无 object → 用 `gnd:<groundId>` 作为匹配键
3. 最后兜底 `ANY`

**理由**: 优先级清晰——有物体时只能对物体操作（如砍石头），无物体时才能对地面操作（如翻耕）。

### D7: 旧存档迁移

**选择**: 服务器端加载存档时检测格式——如果是旧单层格式，自动拆分为 ground + objects：
- GRASS → ground:GRASS, obj:None
- DIRT → ground:DIRT, obj:None
- WATER → ground:WATER, obj:None
- STONE → ground:GRASS, obj:STONE
- TILLED → ground:TILLED, obj:None
- CROP_GROWING → ground:TILLED, obj:CROP_GROWING
- CROP_READY → ground:TILLED, obj:CROP_READY

## Risks / Trade-offs

**[Risk] Sprite 绘制增加**
→ 地物层每个非 None 格额外 1 次 surface.blit 调用。
→ Mitigation: 视口内可见格约 475，PyGame 完全可承受。

**[Risk] 存档体积增加**
→ 多了一层 objects 数组。
→ Mitigation: objects 大部分是 None，序列化后体积增量很小。

**[Trade-off] 地面保持纯色**
→ 不做地面精灵化。可接受：地面层视觉重要性低于地物，纯色 + 2.5D 效果已经足够。
