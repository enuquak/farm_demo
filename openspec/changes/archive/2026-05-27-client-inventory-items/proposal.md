## Why

Farm Demo 目前只有移动和场景渲染能力——玩家在地图上行走但无法与世界产生有意义的交互。需要一个背包与物品系统，让玩家可以持有工具、采集资源、种植作物，形成"获取 → 持有 → 使用 → 产出"的完整游戏循环。

## What Changes

- **新增物品定义层（服务端）**：ItemRegistry 静态配置表，定义物品类型（木材、石头、斧头、锄头、种子、面包、作物），含类型、堆叠上限、使用效果
- **新增背包系统（服务端）**：Inventory 类，30 格（10 快捷栏 + 20 扩展），支持物品增删、堆叠合并、持久化到 DBMgr
- **新增快捷栏 UI（客户端）**：底部固定的 10 格工具栏（PyGame 渲染），数字键 1~0 切换选中格，显示物品图标和数量
- **新增背包面板 UI（客户端）**：按 E 键呼出/关闭的完整背包面板，30 格网格布局
- **新增物品使用系统**：客户端发送使用请求，服务器验证后执行效果（改变 tile、产出物品、消耗物品）
- **新增掉落物系统**：世界中的可拾取物品实体，靠近玩家自动吸附入背包
- **新增作物生长系统（服务端）**：CropSystem 管理种植中的作物，服务端计时器驱动成熟
- **扩展 Tile 类型**：新增 TILLED（翻耕土地）、CROP_GROWING（种植中）、CROP_READY（成熟作物）
- **扩展存档数据**：inventory + crops 数据纳入 DBMgr 持久化

## Capabilities

### New Capabilities

- `item-registry`: 物品定义注册表——静态配置所有物品的 ID、名称、类型、堆叠上限、使用效果映射
- `inventory`: 背包数据模型——30 格 slot 数组、堆叠逻辑、增删查改、快捷栏 activeSlot 管理、服务端持久化
- `inventory-ui`: 背包界面——底部快捷栏 + 完整背包面板，PyGame 像素风格渲染
- `item-interaction`: 物品使用与掉落——客户端发送使用请求，服务器验证并执行效果；掉落物实体在世界中渲染并自动拾取
- `crop-system`: 作物生长系统——管理 TILLED → CROP_GROWING → CROP_READY 的生命周期，服务端计时器驱动成熟

### Modified Capabilities

- `scene-grid`: TileMap 扩展 3 种新 tile 类型
- `player-entity`: Player 数据扩展 inventory 和 crops 字段
- `game-server`: 新增物品使用消息处理、掉落物管理、作物生长逻辑
- `dbmgr`: 持久化 inventory 和 crops 数据

## Impact

- **新增文件**: `scripts/client/ui/hotbar.py`、`scripts/client/ui/inventory_panel.py`、`scripts/client/drop_item.py`、`scripts/server/item_registry.py`、`scripts/server/inventory.py`、`scripts/server/crop_system.py`、`scripts/server/item_interaction.py`
- **修改文件**: `scripts/client/main.py`（UI 初始化 + 键位绑定）、`scripts/server/`（物品消息处理）
- **协议变更**: 新增物品交互消息（背包同步、使用请求、掉落物同步等 MsgID 待分配）
- **存档格式变更**: Player 数据新增 inventory 和 crops 字段
