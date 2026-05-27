## Why

Farm Demo 当前只有一张地图，没有"场景"的概念。牧场类游戏体验需要多个独立区域（农场、家、小镇等），通过门/入口在场景之间切换。本次变更引入多场景框架，实现农场 ↔ 家两个场景的切换，并为后续扩展（小镇、矿洞等）打好架构基础。

## What Changes

- **新增 SceneManager（客户端）**：管理多个场景的渲染切换、过渡动效
- **新增 Scene 注册表**：定义场景配置（地图生成、Portal 位置、出生点）
- **新增 Portal 系统**：玩家站在 Portal tile 上继续往触发方向移动，或在门旁按空格交互，触发场景切换请求
- **新增 Iris 圆形遮罩过渡动效**：以玩家为圆心，收缩→切场景→展开，PyGame surface mask 实现
- **新增"家"室内场景**：10×8 小地图，包含床、电视、炉子等家具地物（本版本仅装饰，预留交互接口）
- **新增 Ground/Object 类型**：WALL（墙壁，不可通行）、WOOD_FLOOR（木地板，可通行）、DOOR_IN/DOOR_OUT/BED/TV/STOVE
- **场景冻结/恢复机制（服务端）**：离开场景时冻结，重新进入时根据时间间隔补帧（如作物生长）
- **存档格式升级**：从单场景升级为多场景结构

## Capabilities

### New Capabilities

- `scene-management`: 场景管理框架（SceneManager、Scene 注册表、场景切换流程、冻结/恢复）
- `portal-system`: Portal 传送门系统（Portal 数据结构、双触发检测、场景间连接）
- `scene-transition`: 场景过渡动效（Iris 圆形遮罩、状态机）
- `house-scene`: "家"室内场景定义（地图生成、家具布局、新地面/地物类型）

### Modified Capabilities

- `scene-grid`: 适配多场景渲染，TileMap 可切换
- `item-interaction`: 新增门交互逻辑（检测 DOOR 地物 → 发送场景切换请求）
- `tile-layers`: 新增 Ground 类型（WALL、WOOD_FLOOR）和 Object 类型（DOOR_IN、DOOR_OUT、BED、TV、STOVE）
- `game-server`: 新增场景切换处理、场景冻结/恢复逻辑
- `player-entity`: Player 数据扩展 activeScene 字段

## Impact

- **新增文件**: `scripts/client/scene/scene_manager.py`、`scripts/client/scene/scene_defs.py`、`scripts/client/scene/scene_transition.py`、`scripts/client/scene/portal.py`
- **修改文件**: `scripts/client/main.py`（SceneManager 集成）、`scripts/client/scene/constants.py`（新类型 + 精灵）、`scripts/server/`（场景切换处理）
- **存档格式变更**: Player 数据升级为多场景结构
