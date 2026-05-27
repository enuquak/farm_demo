## Why

Farm Demo 客户端当前仅支持键盘输入（WASD 移动），交互系统通过空格键触发、目标固定为角色朝向前方 1 格。所有交互缺乏距离校验。需要扩展为支持鼠标点击任意 tile 进行交互，并统一加上九宫格范围限制，提升操作体验。

## What Changes

- **新增输入映射抽象层**：Action Map 将多个物理键归并到语义动作（interact、open_inventory 等）
- **新增鼠标状态追踪**：PyGame 鼠标位置、按键状态、justPressed 检测
- **新增屏幕→世界坐标转换**：Camera 支持将屏幕像素坐标转为世界网格坐标
- **新增鼠标点击交互入口**：鼠标左键点击 tile 可触发物品交互（与空格键等效）
- **统一距离校验**：所有交互路径使用切比雪夫距离校验（九宫格范围）
- **ITEM_EFFECTS 扩展**：每个 effect 新增 interactRange 配置（1=九宫格, -1=无限制）

## Capabilities

### New Capabilities

- `input-mapping`: 输入映射系统——Action Map 抽象层 + 鼠标状态追踪 + 屏幕→世界坐标转换

### Modified Capabilities

- `item-interaction`: 新增鼠标点击交互入口，所有交互路径增加 interactRange 距离校验
- `item-registry`: ITEM_EFFECTS 每条 effect 新增 interactRange 字段

## Impact

- **新增文件**: `scripts/client/input_manager.py`（输入管理器 + Action Map）
- **修改文件**: `scripts/client/main.py`（鼠标事件绑定）、`scripts/client/scene/camera.py`（screenToWorld）、`scripts/server/item_interaction.py`（距离校验）
