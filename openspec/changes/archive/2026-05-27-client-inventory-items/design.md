## Context

Farm Demo 已有场景网格渲染和玩家移动能力。需要加入背包与物品系统，实现完整的游戏交互闭环。在 C++ 服务器 + Python/PyGame 客户端架构下，物品逻辑在服务端，UI 渲染在客户端。

## Goals / Non-Goals

**Goals:**
- 实现物品配置表驱动的物品系统，易于扩展新物品
- 实现 30 格背包（10 快捷栏 + 20 扩展），支持堆叠
- 实现 PyGame 像素风 UI（快捷栏 + 背包面板）
- 实现物品使用请求 → 服务器验证 → 效果执行的完整流程
- 实现工具采集（斧头砍石头 → 获得石头资源）
- 实现简单种植流程（锄头翻耕 → 种子种植 → 成熟 → 收割）
- 实现掉落物在世界中渲染并自动拾取
- 数据持久化到 DBMgr

**Non-Goals:**
- 不实现物品拖拽排列/交换格子位置
- 不实现物品丢弃功能
- 不实现装备系统/属性加成
- 不实现制作合成（crafting）
- 不实现商店/NPC 交易
- 不实现物品品质/稀有度

## Decisions

### D1: 物品定义 — 服务端静态配置表 + 效果映射

**选择**: `scripts/server/item_registry.py` 导出物品定义和效果表

```python
ITEM_DEFS = {
    1: {"name": "木材", "type": "RESOURCE", "max_stack": 99},
    2: {"name": "石头", "type": "RESOURCE", "max_stack": 99},
    3: {"name": "斧头", "type": "TOOL",     "max_stack": 1},
    4: {"name": "锄头", "type": "TOOL",     "max_stack": 1},
    5: {"name": "种子", "type": "SEED",     "max_stack": 99},
    6: {"name": "面包", "type": "FOOD",     "max_stack": 20},
    7: {"name": "作物", "type": "RESOURCE", "max_stack": 99},
}

ITEM_EFFECTS = {
    3: {"obj:STONE": {"remove_object": True, "drops": [{"id": 2, "min": 1, "max": 3}]}},
    4: {"gnd:GRASS": {"set_ground": "TILLED"}, "gnd:DIRT": {"set_ground": "TILLED"}},
    5: {"gnd:TILLED": {"place_object": "CROP_GROWING", "consume_self": True}},
    6: {"ANY": {"consume_self": True, "energy_restore": 15}},
}
```

**理由**: 数据驱动，新增物品只需加配置。服务端集中管理，客户端只做展示。

### D2: 背包数据模型 — 服务端管理

**选择**: `scripts/server/inventory.py`，固定 30 格 slot 数组

```python
class Inventory:
    def __init__(self):
        self.slots = [None] * 30  # [{item_id, count}, ...]
        self.active_slot = 0

    def add_item(self, item_id, count=1) -> int:  # 返回剩余
    def remove_item(self, item_id, count=1) -> bool:
    def get_active_item(self) -> dict | None:
    def serialize(self) -> dict:
    def deserialize(cls, data: dict) -> 'Inventory':
```

**背包同步协议**: 背包变更时服务器发送 InventorySync 消息，包含完整背包数据。客户端只做展示。

### D3: 物品图标 — Palette Sprite → PyGame Surface

**选择**: 客户端为每种物品预生成 8×8 像素图标 Surface

```python
# 物品图标 palette 数据（硬编码在客户端 constants.py）
ITEM_ICONS = {
    1: {"size": 8, "palette": [...], "pixels": [...]},  # 木材
    2: {"size": 8, "palette": [...], "pixels": [...]},  # 石头
    ...
}

# 启动时生成 Surface
icon_surface = pygame.Surface((8, 8))
for y, row in enumerate(pixels):
    for x, idx in enumerate(row):
        icon_surface.set_at((x, y), palette[idx])
icon_surface = pygame.transform.scale(icon_surface, (32, 32))  # 4x 放大
```

**理由**: 与 PyGame 渲染方式一致，Surface 可直接 blit 到屏幕。

### D4: UI 设计 — PyGame 像素风

**选择**: 两层 UI，都用 PyGame 绘制

```
┌───────────────── 游戏画面 ──────────────────┐
│                                             │
│      (TileMap + Player 渲染区域)            │
│                                             │
│                                             │
├─────────────────────────────────────────────┤
│  Hotbar (固定底部, 最后渲染)                │
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐          │
│  │🪓│⛏│🌱│🍞│  │  │  │  │  │  │ 1~0 键  │
│  │×1│×1│×5│×3│  │  │  │  │  │  │          │
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘          │
│        ▲ 选中高亮边框                       │
└─────────────────────────────────────────────┘

InventoryPanel (按 E 切换, 覆盖在游戏画面上)
┌─────────────────────────────────────────────┐
│  背包                               [×] 关闭│
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐          │
│  │  │  │  │  │  │  │  │  │  │  │ 快捷栏   │
│  ├──┼──┼──┼──┼──┼──┼──┼──┼──┼──┤          │
│  │  │  │  │  │  │  │  │  │  │  │ 扩展行1  │
│  ├──┼──┼──┼──┼──┼──┼──┼──┼──┼──┤          │
│  │  │  │  │  │  │  │  │  │  │  │ 扩展行2  │
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘          │
└─────────────────────────────────────────────┘
```

**交互规则**:
- 数字键 1~9, 0 → 切换 activeSlot（发送 ActiveSlotChange 消息）
- E 键 → 切换背包面板（客户端本地状态）
- 背包打开时禁止角色移动

### D5: 物品使用流程

```
客户端按空格键:
  1. 发送 ItemUseReq { direction } 给服务器

服务器收到 ItemUseReq:
  2. 获取 player 的 active item
  3. 计算面前 tile 坐标
  4. 查找 ITEM_EFFECTS 匹配
  5. 验证通过 → 执行效果（修改 tile、增减物品、能量消耗）
  6. 发送 ItemUseResp { success, drops, inventory_sync, map_sync }

客户端收到 ItemUseResp:
  7. 更新 UI 显示
  8. 播放掉落物动画（如有）
```

**理由**: 服务器权威，防止作弊。客户端只发意图，服务器验证并执行。

### D6: 掉落物系统

**选择**: 服务端管理掉落物实体列表，客户端渲染

```python
# 服务端
class DropItem:
    x, y: float          # 世界坐标
    item_id: int
    count: int
    lifetime: float       # 存在时间（秒），超时消失 (300s)

# 掉落物拾取：玩家靠近（距离 < 48px）→ 自动拾取 → 服务器发送拾取消息

# 客户端
class DropItemRenderer:
    def render(self, surface, drop_items, camera):
        for item in drop_items:
            # 绘制物品图标 + 上下浮动动画
            bob_offset = math.sin(time.time() * 3) * 2
            surface.blit(icon, (item.x - cam_x, item.y - cam_y + bob_offset))
```

### D7: 作物生长系统

**选择**: `scripts/server/crop_system.py`，服务端计时器驱动

```python
class CropSystem:
    def __init__(self):
        self.growing_tiles = {}  # key="x,y" → {planted_at, grow_time}

    def plant(self, x, y):
        self.growing_tiles[f"{x},{y}"] = {"planted_at": time.time(), "grow_time": 60}

    def update(self, tile_map):
        for key, crop in list(self.growing_tiles.items()):
            if time.time() - crop["planted_at"] >= crop["grow_time"]:
                x, y = map(int, key.split(","))
                tile_map.set_object(x, y, ObjectType.CROP_READY)
                del self.growing_tiles[key]
```

**成熟时间**: 60 秒（实时），足够短让玩家能体验到完整循环。

### D8: 消息协议

```protobuf
// 背包同步（服务器 → 客户端）
message InventorySync {
    repeated InventorySlot slots = 1;
    int32 active_slot = 2;
}

// 物品使用请求（客户端 → 服务器）
message ItemUseReq {
    string direction = 1;
}

// 物品使用响应（服务器 → 客户端）
message ItemUseResp {
    int32 code = 1;
    repeated DropItemSync drops = 2;
    InventorySync inventory = 3;
    MapChangeSync map_changes = 4;
}

// 掉落物同步
message DropItemSync {
    int32 item_id = 1;
    int32 count = 2;
    float x = 3;
    float y = 4;
    int32 action = 5;  // 0=spawn, 1=pickup
}
```

### D9: 文件结构

```
scripts/server/
├── item_registry.py      # 物品定义表 + 效果表
├── inventory.py          # 背包数据模型
├── item_interaction.py   # 物品使用逻辑
├── drop_item.py          # 掉落物实体管理
├── crop_system.py        # 作物生长系统

scripts/client/
├── ui/
│   ├── hotbar.py         # 快捷栏 UI
│   └── inventory_panel.py # 背包面板 UI
├── drop_item_renderer.py # 掉落物渲染
```

## Risks / Trade-offs

**[Risk] 背包同步频率**
→ 每次变更全量同步，数据量小（30 slots），可接受。
→ 后续可优化为增量同步。

**[Risk] 作物实时计时**
→ 离线时间不计入生长。MVP 可接受，后续可用服务端持久化时间戳。

**[Trade-off] 服务端处理所有逻辑**
→ 增加一次网络往返，但保证数据一致性。局域网延迟 <1ms，无感知。
