## Why

Farm Demo 的场景网格系统初始实现为单层模型——每个 tile 就是一整块纯色方块（包括石头、作物等）。石头表现为"整格灰色"而不是"草地上放了一块石头"，视觉效果不自然。需要将地图拆成地面层（Ground）和地物层（Object），地物使用像素精灵渲染（带透明通道，底层地面透出来），实现"物体放在地面上"的自然效果。

## What Changes

- **拆分 TileMap 数据结构**：单层 `_data` 拆为 `_ground` + `_objects` 双层数组
- **分层渲染**：先渲染地面层（纯色 + 2.5D 高光/阴影），再渲染地物层（16×16 像素精灵）
- **扩展地物类型**：石头、作物、树木等地物使用像素精灵，支持透明通道
- **适配交互系统**：物品交互目标从 tileType 改为 object/ground 双层匹配
- **适配存档**：服务器端地图数据格式升级为双层结构

## Capabilities

### New Capabilities

- `tile-layers`: 地图双层模型——地面层 + 地物层分离，地物像素精灵渲染

### Modified Capabilities

- `scene-grid`: TileMap 数据结构与渲染逻辑重构为双层
- `item-interaction`: 交互目标从 tileType 改为 object/ground 双层匹配（后续迭代）
- `item-registry`: ITEM_EFFECTS 效果类型扩展（removeObject/placeObject/setGround）

## Impact

- **修改文件**: `scripts/client/scene/tile_map.py`（双层数据 + 分层渲染）、`scripts/client/scene/constants.py`（类型定义扩展）
- **存档格式变更**: 地图数据从单层升级为 ground + objects 双层，旧数据自动迁移
