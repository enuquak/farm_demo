## Context

Farm Demo 客户端当前实现了 PyGame 登录界面和 C++ 服务器的 TCP 连接。登录成功后缺少游戏世界的可视化。需要在客户端实现 TileMap 网格渲染系统，作为所有游戏可视化功能的基础。

客户端使用 Python + PyGame，服务器使用 C++ + libevent + protobuf。地图数据存储在服务端，客户端通过协议同步。

## Goals / Non-Goals

**Goals:**
- 实现基于 PyGame 的方形网格地图渲染，支持多种地形类型
- 实现以玩家为中心的正交投影相机，地图边缘约束
- 地图数据从服务器同步，客户端本地缓存渲染
- 地形使用纯色占位符渲染，预留纹理替换扩展点
- 网格坐标系与像素坐标系互转

**Non-Goals:**
- 不实现精灵图/纹理贴图（纯色占位符即可）
- 不实现地图编辑器或外部地图加载
- 不实现碰撞检测（后续迭代）
- 不实现视锥裁剪优化（当前地图规模不需要）
- 不实现小地图/缩放

## Decisions

### D1: 渲染架构 — PyGame Surface 分层

**选择**: 使用 PyGame 的 Surface 分层渲染

```
渲染管线:
  screen.fill(BACKGROUND)
  camera_surface = screen.subsurface(camera.viewport)
  ├── 遍历可见 tile → pygame.draw.rect() 绘制地面
  └── 绘制玩家精灵
  pygame.display.flip()
```

**替代方案**: 使用 PyGame 的 sprite.Group 批量管理。
**理由**: TileMap 渲染不需要 sprite 的更新/碰撞功能，直接 draw.rect 更高效且可控。

### D2: 坐标系统 — 网格坐标 + 像素坐标

**选择**: 双坐标系并存

```
网格坐标: (tileX, tileY) — 逻辑位置
像素坐标: (pixelX, pixelY) — 渲染位置

转换:
  pixelX = tileX * TILE_SIZE
  pixelY = tileY * TILE_SIZE
  tileX = pixelX // TILE_SIZE
  tileY = pixelY // TILE_SIZE

TILE_SIZE = 32 像素
```

**理由**: 网格坐标用于游戏逻辑（碰撞、交互），像素坐标用于渲染和物理移动。32 像素格子在现代屏幕上大小合适。

### D3: 相机跟随 — 直接锁定 + 边界约束

**选择**: 相机中心直接锁定玩家位置，在地图边缘做边界约束

```
camera.x = clamp(player.centerX - viewportW/2, 0, mapPixelW - viewportW)
camera.y = clamp(player.centerY - viewportH/2, 0, mapPixelH - viewportH)
```

**替代方案**: 平滑插值跟随（lerp）。
**理由**: 直接锁定最简单，后续可轻松加 lerp。MVP 阶段保持简单。

### D4: 地图数据同步 — 进入时全量下发

**选择**: 玩家登录成功后，服务器一次性下发完整地图数据。客户端缓存在本地。

```
协议流程:
  1. 客户端发送 EnterGameReq
  2. 服务器回复 EnterGameResp（包含 player_data）
  3. 服务器发送 MapDataNotify（包含 ground[][] + objects[][]）
  4. 客户端构建 TileMap 并开始渲染
```

**替代方案**: 按区域增量加载（类似 Chunk）。
**理由**: 当前地图规模（60×50 = 3000 格）数据量很小，全量下发简单可靠。后续地图变大时可升级为 Chunk 加载。

### D5: 地形类型定义

**选择**: 在 constants.py 中定义 GroundType 枚举

```python
class GroundType:
    GRASS = 0    # 草地 → 绿色 #4a7c2e
    DIRT  = 1    # 泥土 → 棕色 #8b6914
    WATER = 2    # 水面 → 蓝色 #2e6bb0 (不可通行)
    SAND  = 3    # 沙地 → 黄色 #c2b280
    TILLED = 4   # 翻耕 → 深棕 #5a3a1a
```

每种地形关联：颜色、是否可通行、2.5D 高光/阴影色值。

### D6: 模块结构

```
scripts/client/scene/
├── __init__.py
├── constants.py      # 地形类型、尺寸常量、颜色表
├── tile_map.py       # TileMap 数据结构 + 渲染
├── camera.py         # 正交投影相机 + 坐标转换
└── renderer.py       # 场景渲染器（组合 TileMap + Camera + Player）
```

**职责划分**:
- `constants.py` — 纯数据定义
- `tile_map.py` — 地图数据管理 + 单层渲染
- `camera.py` — 视口管理 + 坐标转换
- `renderer.py` — 渲染编排（调用 TileMap 和 Player 的渲染方法）

## Risks / Trade-offs

**[Risk] PyGame 性能**
→ 每帧遍历可见 tile 并 draw.rect，60×50 地图约 3000 次 draw 调用。
→ Mitigation: 视口内可见格约 25×19=475，PyGame 完全可承受。后续可加 dirty rect 优化。

**[Risk] 地图数据同步延迟**
→ 大地图全量下发可能有延迟。
→ Mitigation: 当前地图很小（~3KB），无问题。后续升级为 Chunk 加载。

**[Trade-off] 纯色占位 vs 纹理**
→ 纯色渲染视觉效果有限，但实现简单且后续可平滑替换为纹理。
→ 纹理替换只需在 constants.py 中扩展颜色表为纹理表，渲染逻辑不变。

## Open Questions

- [ ] 窗口大小是否固定 800×600，还是支持 resize？
- [ ] 地图网格线是否需要显示（开发阶段辅助线）？
