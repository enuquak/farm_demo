## Context

Farm Demo 当前只有一张地图。需要引入多场景框架，支持多个独立区域（农场、家等）之间的切换。在客户端-服务器架构下，场景数据由服务端管理，客户端负责渲染切换和过渡动效。

## Goals / Non-Goals

**Goals:**
- 可扩展的多场景框架，任意数量场景通过注册表定义
- 农场 ↔ 家 两个可玩场景，通过门连接
- Portal 双触发：站在门上往外走 OR 门旁按空格/鼠标交互
- Iris 圆形遮罩过渡动效
- 冻结/恢复机制：离开场景时冻结，回来时按 elapsed 补帧
- 旧存档兼容迁移

**Non-Goals:**
- 本版本不实现家具功能（睡觉/烹饪/看电视），仅预留接口
- 不实现 NPC / 小镇 / 矿洞等后续场景
- 不实现场景之间的声音/BGM 切换
- 不实现无缝地图（硬切换）

## Decisions

### D1: 架构拆分 — 客户端 SceneManager + 服务端场景路由

**选择**: 客户端 SceneManager 管理渲染切换，服务端管理场景数据和逻辑

```
客户端 SceneManager:
├── 持有当前场景的 TileMap 引用
├── 管理场景切换流程（过渡动效）
├── 处理 Portal 检测（客户端触发，发送请求给服务器）
└── 渲染当前场景

服务端:
├── 管理每个场景的数据（TileMap、CropSystem、DropItems）
├── 处理场景切换请求
├── 管理场景冻结/恢复
└── 下发场景数据给客户端
```

**理由**: 场景数据和逻辑在服务端保证一致性，客户端只做渲染。

### D2: Portal 触发机制

**选择**: Portal tile 是 walkable 的，玩家可以站在上面。触发条件：玩家当前所在 tile 是 Portal 且正在按 portal.trigger_dir 方向移动。

```
客户端检测:
  1. 玩家移动到 Portal tile 上
  2. 检查移动方向是否匹配 portal.trigger_dir
  3. 匹配 → 发送 SceneChangeReq { target_scene, target_portal_id }

服务器处理:
  1. 验证 Portal 存在且合法
  2. 冻结当前场景
  3. 加载目标场景（或解冻）
  4. 设置玩家位置到目标出生点
  5. 发送 SceneChangeResp + 新场景的 MapData
```

### D3: 门的双触发 — 走出去 + 空格交互

**选择**: 除了"站在门上走出去"，还支持"站在门旁对门按空格"触发切换。

客户端检测:
- 在 item_interaction 中检测 objectType 的 interact_type
- `'portal'` → 发送 SceneChangeReq
- `'furniture'` → 预留接口（本版本空实现）

### D4: 过渡动效 — Iris 圆形遮罩 (PyGame Surface Mask)

**选择**: 使用 PyGame Surface 操作实现圆形遮罩

```python
class SceneTransition:
    def __init__(self):
        self.state = "IDLE"  # IDLE → IRIS_CLOSE → SWITCHING → IRIS_OPEN → IDLE
        self.center = (0, 0)  # 玩家屏幕位置
        self.radius = 0
        self.max_radius = 0

    def start(self, player_screen_pos):
        self.state = "IRIS_CLOSE"
        self.center = player_screen_pos
        self.max_radius = math.sqrt(SCREEN_W**2 + SCREEN_H**2) / 2
        self.radius = self.max_radius

    def update(self, dt):
        if self.state == "IRIS_CLOSE":
            self.radius -= self.max_radius * dt / 0.3  # 300ms
            if self.radius <= 0:
                self.state = "SWITCHING"
        elif self.state == "SWITCHING":
            self.state = "IRIS_OPEN"
        elif self.state == "IRIS_OPEN":
            self.radius += self.max_radius * dt / 0.3
            if self.radius >= self.max_radius:
                self.state = "IDLE"

    def apply(self, surface):
        # 创建遮罩 surface
        mask = pygame.Surface((SCREEN_W, SCREEN_H), pygame.SRCALPHA)
        mask.fill((0, 0, 0, 255))
        pygame.draw.circle(mask, (0, 0, 0, 0), self.center, int(self.radius))
        surface.blit(mask, (0, 0))
```

**状态机**: IDLE → IRIS_CLOSE (300ms) → SWITCHING (1帧) → IRIS_OPEN (300ms) → IDLE

### D5: 场景注册表 — SceneDefs

**选择**: 每个场景在 `scene_defs.py` 中注册

```python
SCENE_DEFS = {
    "farm": {
        "width": 60, "height": 50,
        "generate": generate_farm_map,  # 返回 {ground, objects}
        "portals": [
            {"pos": (25, 48), "trigger_dir": "down", "target": "house", "target_portal": "door_out"},
            {"pos": (26, 48), "trigger_dir": "down", "target": "house", "target_portal": "door_out"},
        ],
        "player_spawn": (30, 25),
    },
    "house": {
        "width": 10, "height": 8,
        "generate": generate_house_map,
        "portals": [
            {"pos": (5, 7), "trigger_dir": "down", "target": "farm", "target_portal": "door_in"},
        ],
        "player_spawn": (5, 6),
    },
}
```

### D6: 冻结/恢复机制 — 服务端

**选择**: 服务端每个场景维护 `frozen_at` 时间戳

```python
class SceneState:
    def __init__(self):
        self.tile_map = None
        self.crop_system = CropSystem()
        self.drop_items = []
        self.frozen_at = None  # None = 活跃, timestamp = 冻结时间

    def freeze(self):
        self.frozen_at = time.time()

    def thaw(self):
        if self.frozen_at:
            elapsed = time.time() - self.frozen_at
            self.crop_system.simulate_elapsed(elapsed)
            self.frozen_at = None
```

### D7: 存档格式升级

```json
{
    "active_scene": "farm",
    "scenes": {
        "farm": {"ground": [...], "objects": [...], "crops": {...}, "drops": [], "frozen_at": null},
        "house": {"ground": [...], "objects": [...], "crops": {...}, "drops": [], "frozen_at": null}
    }
}
```

旧存档（单场景）→ 包装为 `scenes.farm`，`active_scene = "farm"`。

### D8: 新 Ground/Object 类型

```python
class GroundType:
    # ... 现有类型
    WALL = 5         # 墙壁，不可通行
    WOOD_FLOOR = 6   # 木地板，可通行

class ObjectType:
    # ... 现有类型
    DOOR_IN = 10     # 室内门
    DOOR_OUT = 11    # 室外门
    BED = 12         # 床
    TV = 13          # 电视
    STOVE = 14       # 炉子
```

BED/TV/STOVE 标记 `interactable=True, interact_type='furniture'`。
DOOR_IN/DOOR_OUT 标记 `interactable=True, interact_type='portal'`。

### D9: 文件结构

```
scripts/client/scene/
├── scene_manager.py      # 场景管理器（渲染切换、Portal 检测）
├── scene_defs.py         # 场景注册表
├── scene_transition.py   # Iris 过渡动效
└── portal.py             # Portal 数据结构

scripts/server/
├── scene_manager.py      # 服务端场景管理（数据、冻结/恢复）
```

## Risks / Trade-offs

**[Risk] 场景切换时客户端/服务端状态不一致**
→ 过渡期间输入屏蔽，场景切换完成后再恢复输入。
→ Mitigation: SceneManager 在非 IDLE 状态时直接 return，不转发任何输入。

**[Risk] 过渡期间网络中断**
→ 场景切换请求发出但响应丢失。
→ Mitigation: 超时重试 + 回退到当前场景。

**[Trade-off] PyGame Surface Mask 性能**
→ 每帧创建遮罩 Surface 有一定开销。
→ Mitigation: 仅在过渡期间创建，IDLE 时不创建。300ms 过渡期间的开销可接受。
