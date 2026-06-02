# 矿洞场景系统设计（Phase 2）

**日期**: 2026-06-02
**状态**: 设计中
**范围**: 矿洞入口、多层场景切换、程序化地图生成、光照系统、怪物刷新机制

---

## 1. 概述

为怪兽与战斗系统添加矿洞场景，实现多层矿洞探索玩法。玩家从农场传送门进入矿洞，逐层深入挑战更强敌人，最终到达 Boss 房间。

### 1.1 功能清单

| 功能 | 描述 |
|------|------|
| 矿洞入口 | 农场边缘传送门，玩家走到自动触发场景切换 |
| 多层矿洞 | 4 层矿洞（入口、深层、骷髅大厅、Boss 房间） |
| 程序化地图 | 使用房间+走廊算法随机生成矿洞地图 |
| 光照系统 | 多层光照效果（环境光、玩家光照、静态光源、动态效果） |
| 怪物刷新 | 延迟刷新、避开玩家、场景切换冻结/解冻 |
| 场景状态 | 保存/恢复矿洞场景状态（怪物、进度） |

### 1.2 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 矿洞入口触发方式 | 传送门自动触发 | 简化交互，提升探索流畅感 |
| 地图生成方式 | 程序化生成 | 增加重玩性，减少手动设计工作量 |
| 光照实现方式 | Pygame Surface 混合 | 与现有渲染管线兼容，性能可控 |
| 层数设计 | 4 层（含 Boss 房间） | 保持与原设计文档一致 |

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      GameScene                          │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐    ┌───────────┐ │
│  │ Cave         │───▶│ Cave         │───▶│ Cave      │ │
│  │ Manager      │    │ Generator    │    │ Lighting  │ │
│  └──────────────┘    └──────────────┘    └───────────┘ │
│       │                    │                   │        │
│       ▼                    ▼                   ▼        │
│  ┌──────────────┐    ┌──────────────┐    ┌───────────┐ │
│  │ Monster      │    │ Scene        │    │ Game      │ │
│  │ Manager      │    │ Manager      │    │ Renderer  │ │
│  └──────────────┘    └──────────────┘    └───────────┘ │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐                   │
│  │ Combat       │    │ Battle       │                   │
│  │ System       │    │ UI           │                   │
│  └──────────────┘    └──────────────┘                   │
└─────────────────────────────────────────────────────────┘
```

### 2.1 模块职责

| 模块 | 职责 |
|------|------|
| **CaveManager** | 矿洞场景管理，协调地图生成、怪物刷新、光照渲染 |
| **CaveGenerator** | 程序化地图生成，创建房间、走廊、放置对象 |
| **CaveLighting** | 矿洞光照系统，实现多层光照效果 |
| **CaveSpawner** | 服务器怪物刷新器，管理矿洞怪物生命周期 |
| **SceneState** | 场景状态保存/恢复，支持矿洞场景数据 |

### 2.2 数据驱动

矿洞层配置、光源定义、刷新参数全部用 JSON 配置文件定义，代码为通用引擎。与现有怪物/武器配置架构一致。

---

## 3. 矿洞入口与场景切换

### 3.1 矿洞入口（农场→矿洞）

**入口位置**：农场地图边缘放置一个传送门区域（`ObjectType.CAVE_PORTAL`）

**触发方式**：玩家走到传送门区域上自动触发场景切换，无需按键交互

**切换流程**：
```
玩家移动到传送门区域
       │
       ▼
客户端检测到玩家在 CAVE_PORTAL 类型的格子上
       │
       ▼
客户端发送场景切换请求（MSG_ID_SCENE_CHANGE_REQ）
       │
       ▼
服务器验证：玩家确实在传送门位置
       │
       ▼
服务器保存农场场景状态，加载矿洞第 1 层
       │
       ▼
客户端收到响应，切换到矿洞场景渲染
       │
       ▼
玩家出现在矿洞第 1 层的入口位置
```

### 3.2 层间切换（楼梯）

**向下楼梯**（`ObjectType.STAIRS_DOWN`）：玩家走到楼梯上自动下楼
**向上楼梯**（`ObjectType.STAIRS_UP`）：玩家走到楼梯上自动上楼

**切换流程**：与矿洞入口相同，只是目标场景不同

### 3.3 返回农场

矿洞第 1 层有向上楼梯，连接回农场传送门位置

### 3.4 新增 ObjectType

```python
ObjectType.CAVE_PORTAL = 20   # 矿洞传送门（农场入口）
ObjectType.STAIRS_DOWN = 21   # 向下楼梯
ObjectType.STAIRS_UP = 22     # 向上楼梯
```

### 3.5 新增消息 ID

```cpp
MSG_ID_SCENE_CHANGE_REQ = 5060,  // 场景切换请求
MSG_ID_SCENE_CHANGE_RESP = 5061, // 场景切换响应
MSG_ID_CAVE_ENTER = 5070,        // 进入矿洞
MSG_ID_CAVE_EXIT = 5071,         // 离开矿洞
MSG_ID_CAVE_LEVEL_CHANGE = 5072  // 矿洞层级切换
```

---

## 4. 矿洞层配置

### 4.1 配置文件（cave_levels.json）

```json
{
  "cave_levels": [
    {
      "level": 1,
      "name": "矿洞入口",
      "scene_id": "cave_1",
      "map_file": "cave_1.tmx",
      "max_monsters": 5,
      "monster_types": ["slime"],
      "spawn_points": [
        {"x": 10, "y": 8},
        {"x": 15, "y": 12},
        {"x": 8, "y": 15}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "ambient_color": [40, 30, 30],
      "light_radius": 5,
      "static_lights": [
        {"x": 5, "y": 10, "radius": 3, "color": [255, 200, 100], "type": "torch"},
        {"x": 12, "y": 5, "radius": 3, "color": [255, 200, 100], "type": "torch"},
        {"x": 18, "y": 15, "radius": 4, "color": [255, 220, 150], "type": "lantern"}
      ],
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 10.0,
        "spawn_check_interval": 5.0,
        "spawn_radius": 2
      }
    },
    {
      "level": 2,
      "name": "矿洞深层",
      "scene_id": "cave_2",
      "map_file": "cave_2.tmx",
      "max_monsters": 8,
      "monster_types": ["slime", "bat"],
      "spawn_points": [
        {"x": 5, "y": 5},
        {"x": 15, "y": 8},
        {"x": 10, "y": 15},
        {"x": 18, "y": 12}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "exit_up": {"x": 2, "y": 18},
      "ambient_color": [30, 25, 25],
      "light_radius": 4,
      "static_lights": [
        {"x": 8, "y": 8, "radius": 3, "color": [255, 200, 100], "type": "torch"},
        {"x": 20, "y": 18, "radius": 3, "color": [255, 200, 100], "type": "torch"}
      ],
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 8.0,
        "spawn_check_interval": 4.0,
        "spawn_radius": 2
      }
    },
    {
      "level": 3,
      "name": "骷髅大厅",
      "scene_id": "cave_3",
      "map_file": "cave_3.tmx",
      "max_monsters": 10,
      "monster_types": ["bat", "skeleton"],
      "spawn_points": [
        {"x": 8, "y": 8},
        {"x": 12, "y": 8},
        {"x": 10, "y": 12},
        {"x": 15, "y": 15},
        {"x": 5, "y": 15}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "exit_up": {"x": 2, "y": 18},
      "ambient_color": [25, 20, 20],
      "light_radius": 3,
      "static_lights": [
        {"x": 10, "y": 5, "radius": 3, "color": [255, 200, 100], "type": "torch"},
        {"x": 25, "y": 25, "radius": 3, "color": [255, 200, 100], "type": "torch"}
      ],
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 6.0,
        "spawn_check_interval": 3.0,
        "spawn_radius": 3
      }
    },
    {
      "level": 10,
      "name": "骷髅王座",
      "scene_id": "cave_boss",
      "map_file": "cave_boss.tmx",
      "max_monsters": 1,
      "monster_types": ["boss_skeleton_king"],
      "spawn_points": [
        {"x": 10, "y": 5}
      ],
      "entrance": {"x": 10, "y": 18},
      "exit_up": {"x": 10, "y": 18},
      "is_boss_room": true,
      "ambient_color": [20, 15, 15],
      "light_radius": 6,
      "static_lights": [],
      "boss_lighting": {
        "color": [150, 50, 50],
        "pulse_speed": 0.5,
        "pulse_range": [0.8, 1.2]
      },
      "spawn_config": {
        "respawn_delay": 0,
        "spawn_check_interval": 0,
        "spawn_radius": 0
      }
    }
  ]
}
```

### 4.2 配置字段说明

| 字段 | 说明 |
|------|------|
| `level` | 层级编号（1-3 为普通层，10 为 Boss 房间） |
| `name` | 显示名称 |
| `scene_id` | 场景唯一标识 |
| `map_file` | 地图文件名（程序化生成时忽略） |
| `max_monsters` | 该层最大怪物数量 |
| `monster_types` | 该层可出现的怪物类型 |
| `spawn_points` | 怪物刷新点坐标（格子坐标） |
| `entrance` | 玩家从上层下来时的出生点 |
| `exit_down` | 向下楼梯位置 |
| `exit_up` | 向上楼梯位置 |
| `is_boss_room` | 是否为 Boss 房间 |
| `ambient_color` | 环境光颜色 [R, G, B] |
| `light_radius` | 玩家光照半径（格） |
| `static_lights` | 静态光源列表 |
| `boss_lighting` | Boss 房间特殊光照配置 |
| `spawn_config` | 怪物刷新参数 |

### 4.3 场景定义扩展

在 `scene_defs.py` 中添加矿洞场景定义：

```python
SCENE_DEFS = {
    # ... 现有场景 ...
    "cave_1": {
        "name": "矿洞入口",
        "map_file": "cave_1.tmx",
        "bg_color": (40, 30, 30),
        "is_cave": True,
    },
    "cave_2": {
        "name": "矿洞深层",
        "map_file": "cave_2.tmx",
        "bg_color": (30, 25, 25),
        "is_cave": True,
    },
    "cave_3": {
        "name": "骷髅大厅",
        "map_file": "cave_3.tmx",
        "bg_color": (25, 20, 20),
        "is_cave": True,
    },
    "cave_boss": {
        "name": "骷髅王座",
        "map_file": "cave_boss.tmx",
        "bg_color": (20, 15, 15),
        "is_cave": True,
        "is_boss_room": True,
    },
}
```

---

## 5. 程序化地图生成

### 5.1 生成算法概述

使用房间+走廊算法随机生成矿洞地图：
1. 在地图区域内随机放置房间（矩形区域）
2. 使用走廊连接相邻房间
3. 在特定房间放置楼梯和刷新点
4. 填充墙壁和地板贴图

### 5.2 CaveGenerator 类设计

```python
class CaveGenerator:
    """矿洞地图程序化生成器"""
    
    def __init__(self, seed: int = None):
        self._rng = random.Random(seed)
    
    def generate(self, level_config: dict) -> CaveMap:
        """
        生成矿洞地图
        
        Args:
            level_config: 矿洞层配置（来自 cave_levels.json）
            
        Returns:
            CaveMap 对象，包含地图数据
        """
        # 1. 确定地图尺寸
        map_width, map_height = self._get_map_size(level_config["level"])
        
        # 2. 生成房间
        rooms = self._generate_rooms(map_width, map_height, 
                                     self._get_room_count(level_config["level"]))
        
        # 3. 连接房间（生成走廊）
        corridors = self._connect_rooms(rooms)
        
        # 4. 放置楼梯
        self._place_stairs(rooms, level_config)
        
        # 5. 放置刷新点
        spawn_points = self._place_spawn_points(rooms, level_config)
        
        # 6. 放置光源
        lights = self._place_lights(rooms, level_config)
        
        # 7. 生成地图数据
        map_data = self._build_map_data(map_width, map_height, rooms, corridors)
        
        return CaveMap(map_width, map_height, map_data, spawn_points, lights)
    
    def _get_map_size(self, level: int) -> tuple:
        """根据层级确定地图尺寸"""
        sizes = {
            1: (20, 20),   # 矿洞入口
            2: (25, 25),   # 矿洞深层
            3: (30, 30),   # 骷髅大厅
            10: (20, 20),  # Boss 房间
        }
        return sizes.get(level, (20, 20))
    
    def _get_room_count(self, level: int) -> int:
        """根据层级确定房间数量"""
        counts = {1: 4, 2: 6, 3: 8, 10: 1}
        return counts.get(level, 4)
    
    def _generate_rooms(self, map_width: int, map_height: int, 
                        room_count: int) -> list:
        """生成随机房间"""
        rooms = []
        attempts = 0
        max_attempts = 100
        
        while len(rooms) < room_count and attempts < max_attempts:
            # 随机房间大小和位置
            w = self._rng.randint(4, 8)
            h = self._rng.randint(4, 8)
            x = self._rng.randint(1, map_width - w - 1)
            y = self._rng.randint(1, map_height - h - 1)
            
            new_room = Room(x, y, w, h)
            
            # 检查是否与现有房间重叠
            if not any(new_room.intersects(r) for r in rooms):
                rooms.append(new_room)
            
            attempts += 1
        
        return rooms
    
    def _connect_rooms(self, rooms: list) -> list:
        """使用走廊连接房间"""
        corridors = []
        
        for i in range(len(rooms) - 1):
            room_a = rooms[i]
            room_b = rooms[i + 1]
            
            # 从房间中心点连接
            ax, ay = room_a.center
            bx, by = room_b.center
            
            # 随机选择先水平还是垂直
            if self._rng.random() < 0.5:
                # 先水平后垂直
                corridors.append(Corridor(ax, ay, bx, ay))
                corridors.append(Corridor(bx, ay, bx, by))
            else:
                # 先垂直后水平
                corridors.append(Corridor(ax, ay, ax, by))
                corridors.append(Corridor(ax, by, bx, by))
        
        return corridors
    
    def _place_stairs(self, rooms: list, level_config: dict):
        """放置楼梯"""
        if not rooms:
            return
        
        # 入口楼梯放在第一个房间
        entrance = level_config.get("entrance")
        if entrance:
            rooms[0].add_object("STAIRS_UP", entrance["x"], entrance["y"])
        
        # 出口楼梯放在最后一个房间
        exit_down = level_config.get("exit_down")
        if exit_down:
            rooms[-1].add_object("STAIRS_DOWN", exit_down["x"], exit_down["y"])
    
    def _place_spawn_points(self, rooms: list, level_config: dict) -> list:
        """放置怪物刷新点"""
        spawn_points = []
        max_monsters = level_config.get("max_monsters", 5)
        
        # 在每个房间放置 1-2 个刷新点
        for i, room in enumerate(rooms):
            if i == 0:  # 跳过入口房间
                continue
            
            count = min(2, max_monsters - len(spawn_points))
            for _ in range(count):
                x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                spawn_points.append({"x": x, "y": y})
        
        return spawn_points[:max_monsters]
    
    def _place_lights(self, rooms: list, level_config: dict) -> list:
        """放置光源"""
        lights = []
        
        # 在每个房间放置 1-2 个火把
        for room in rooms:
            count = self._rng.randint(1, 2)
            for _ in range(count):
                # 放在墙壁附近
                side = self._rng.choice(["top", "bottom", "left", "right"])
                if side == "top":
                    x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                    y = room.y
                elif side == "bottom":
                    x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                    y = room.y + room.h - 1
                elif side == "left":
                    x = room.x
                    y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                else:
                    x = room.x + room.w - 1
                    y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                
                lights.append({
                    "x": x, "y": y,
                    "radius": 3,
                    "color": [255, 200, 100],
                    "type": "torch"
                })
        
        return lights
    
    def _build_map_data(self, map_width: int, map_height: int,
                        rooms: list, corridors: list) -> list:
        """构建地图数据（二维数组）"""
        # 初始化为墙壁
        map_data = [[1 for _ in range(map_width)] for _ in range(map_height)]
        
        # 挖掘房间
        for room in rooms:
            for y in range(room.y, room.y + room.h):
                for x in range(room.x, room.x + room.w):
                    map_data[y][x] = 0  # 地板
        
        # 挖掘走廊
        for corridor in corridors:
            for x, y in corridor.points():
                if 0 <= x < map_width and 0 <= y < map_height:
                    map_data[y][x] = 0
        
        return map_data
```

### 5.3 CaveMap 数据结构

```python
class CaveMap:
    """矿洞地图数据"""
    
    def __init__(self, width: int, height: int, map_data: list,
                 spawn_points: list, lights: list):
        self.width = width
        self.height = height
        self.map_data = map_data  # 二维数组，0=地板，1=墙壁
        self.spawn_points = spawn_points
        self.lights = lights
        self.objects = []  # 楼梯等对象
    
    def is_walkable(self, x: int, y: int) -> bool:
        """检查位置是否可行走"""
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.map_data[y][x] == 0
        return False
```

### 5.4 Boss 房间特殊处理

Boss 房间（level=10）使用特殊生成逻辑：
- 固定尺寸 20x20
- 单个大型房间（无走廊）
- 中心位置放置 Boss 刷新点
- 入口在底部，无向下楼梯

### 5.5 随机种子

- 使用随机种子确保同一配置生成相同地图（可选）
- 也可以每次进入矿洞都生成新地图（增加重玩性）
- 建议：使用配置中的 `seed` 字段，如果未指定则使用随机种子

---

## 6. 矿洞光照系统

### 6.1 光照效果概述

矿洞场景使用多层光照系统，包含：
- **基础环境光**：暗色覆盖，营造矿洞氛围
- **玩家光照**：玩家周围的圆形光照范围
- **静态光源**：火把/灯笼等固定位置的光源
- **动态效果**：火把闪烁、受伤红光、Boss 特殊光照

### 6.2 CaveLighting 类设计

```python
class CaveLighting:
    """矿洞光照系统"""
    
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._light_mask = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
        self._ambient_surface = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
        
        # 动态效果状态
        self._torch_flicker_timer = 0.0
        self._torch_flicker_offsets = {}  # light_id -> radius_offset
        
        # 受伤红光
        self._damage_flash_until = 0.0
        self._damage_flash_intensity = 0
        
        # Boss 光照
        self._boss_pulse_timer = 0.0
        self._boss_lighting_config = None
    
    def trigger_damage_flash(self, intensity: int = 100):
        """触发受伤红光效果"""
        self._damage_flash_until = time.time() + 0.3
        self._damage_flash_intensity = intensity
    
    def set_boss_lighting(self, config: dict):
        """设置 Boss 房间光照配置"""
        self._boss_lighting_config = config
        self._boss_pulse_timer = 0.0
    
    def update(self, dt: float):
        """更新动态效果"""
        # 火把闪烁
        self._torch_flicker_timer += dt
        if self._torch_flicker_timer >= 0.1:  # 每 0.1 秒更新一次
            self._torch_flicker_timer = 0.0
            for light_id in self._torch_flicker_offsets:
                # 随机偏移光照半径 ±0.2 格
                self._torch_flicker_offsets[light_id] = random.uniform(-0.2, 0.2)
        
        # Boss 光照脉冲
        if self._boss_lighting_config:
            self._boss_pulse_timer += dt
    
    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float,
               player_world_x: float, player_world_y: float,
               light_radius: int, ambient_color: tuple,
               static_lights: list = None):
        """
        渲染光照效果
        
        Args:
            screen: 主屏幕 Surface
            camera_x: 相机 X 偏移（世界像素）
            camera_y: 相机 Y 偏移（世界像素）
            player_world_x: 玩家世界坐标 X
            player_world_y: 玩家世界坐标 Y
            light_radius: 玩家光照半径（格数）
            ambient_color: 环境光颜色 (R, G, B)
            static_lights: 静态光源列表
        """
        # 1. 填充黑色（全暗）
        self._light_mask.fill((0, 0, 0, 255))
        
        # 2. 绘制玩家光照
        player_screen_x = int((player_world_x - camera_x) * ZOOM_FACTOR)
        player_screen_y = int((player_world_y - camera_y) * ZOOM_FACTOR)
        radius_px = light_radius * TILE_SIZE * ZOOM_FACTOR
        
        # 使用渐变圆形（中心透明，边缘半透明）
        self._draw_light_circle(self._light_mask, player_screen_x, player_screen_y, 
                               int(radius_px), (255, 255, 255, 0))
        
        # 3. 绘制静态光源
        if static_lights:
            for i, light in enumerate(static_lights):
                lx = int((light["x"] * TILE_SIZE - camera_x) * ZOOM_FACTOR)
                ly = int((light["y"] * TILE_SIZE - camera_y) * ZOOM_FACTOR)
                lr = light["radius"] * TILE_SIZE * ZOOM_FACTOR
                
                # 火把闪烁效果
                if light.get("type") == "torch":
                    flicker = self._torch_flicker_offsets.get(i, 0) * TILE_SIZE * ZOOM_FACTOR
                    lr += int(flicker)
                
                color = tuple(light.get("color", [255, 200, 100]))
                self._draw_light_circle(self._light_mask, lx, ly, int(lr), (*color, 0))
        
        # 4. 叠加光照遮罩
        screen.blit(self._light_mask, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
        
        # 5. 环境光色调
        self._ambient_surface.fill((*ambient_color, 30))
        screen.blit(self._ambient_surface, (0, 0))
        
        # 6. Boss 光照脉冲效果
        if self._boss_lighting_config:
            self._render_boss_lighting(screen, camera_x, camera_y)
        
        # 7. 受伤红光效果
        if time.time() < self._damage_flash_until:
            self._render_damage_flash(screen)
    
    def _draw_light_circle(self, surface: pygame.Surface, x: int, y: int, 
                          radius: int, center_color: tuple):
        """绘制渐变光照圆形"""
        # 简化方案：使用多层同心圆模拟渐变
        for r in range(radius, 0, -2):
            alpha = int(255 * (r / radius))  # 边缘更暗
            color = (*center_color[:3], alpha)
            pygame.draw.circle(surface, color, (x, y), r)
    
    def _render_boss_lighting(self, screen: pygame.Surface, 
                              camera_x: float, camera_y: float):
        """渲染 Boss 房间特殊光照"""
        config = self._boss_lighting_config
        if not config:
            return
        
        # 脉冲效果
        pulse_speed = config.get("pulse_speed", 0.5)
        pulse_range = config.get("pulse_range", [0.8, 1.2])
        
        pulse = math.sin(self._boss_pulse_timer * pulse_speed * math.pi * 2)
        pulse = pulse_range[0] + (pulse_range[1] - pulse_range[0]) * (pulse + 1) / 2
        
        # 全屏红色脉冲
        boss_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        color = config.get("color", [150, 50, 50])
        alpha = int(20 * pulse)
        boss_surface.fill((*color, alpha))
        screen.blit(boss_surface, (0, 0))
    
    def _render_damage_flash(self, screen: pygame.Surface):
        """渲染受伤红光效果"""
        elapsed = time.time() - self._damage_flash_until + 0.3
        alpha = int(self._damage_flash_intensity * (1 - elapsed / 0.3))
        alpha = max(0, min(255, alpha))
        
        flash_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        flash_surface.fill((255, 0, 0, alpha))
        screen.blit(flash_surface, (0, 0))
```

### 6.3 光照效果总结

| 效果 | 实现方式 | 性能影响 |
|------|----------|----------|
| 基础环境光 | 半透明 Surface 叠加 | 低 |
| 玩家光照 | 渐变圆形绘制 | 中 |
| 静态光源 | 多个渐变圆形 | 中 |
| 火把闪烁 | 随机半径偏移 | 低 |
| Boss 脉冲 | 全屏半透明红色 | 低 |
| 受伤红光 | 全屏半透明红色 | 低 |

### 6.4 光照参数

| 层级 | 环境光颜色 | 光照半径 | 效果 |
|------|-----------|----------|------|
| 矿洞入口 | [40, 30, 30] | 5 格 | 较亮，适合新手 |
| 矿洞深层 | [30, 25, 25] | 4 格 | 中等亮度 |
| 骷髅大厅 | [25, 20, 20] | 3 格 | 较暗，增加紧张感 |
| Boss 房间 | [20, 15, 15] | 6 格 | Boss 房间较大，光照范围也大 |

### 6.5 性能优化建议

- 如果帧率下降，可以降低光照更新频率（每 2-3 帧更新一次）
- 缓存光照遮罩，只在玩家移动超过一定距离时重新绘制
- 使用更简单的圆形绘制（不使用渐变）

---

## 7. 矿洞怪物刷新机制

### 7.1 刷新规则

- 每个矿洞层配置最大怪物数量和刷新点
- 怪物死亡后，延迟 N 秒在随机刷新点重新生成
- 玩家离开场景时冻结怪物状态（复用现有 freeze/thaw 机制）
- Boss 房间不自动刷新 Boss

### 7.2 刷新配置

在 `cave_levels.json` 的 `spawn_config` 中定义：

| 字段 | 说明 |
|------|------|
| `respawn_delay` | 怪物死亡后重新刷新的延迟时间（秒） |
| `spawn_check_interval` | 检查刷新条件的间隔（秒） |
| `spawn_radius` | 刷新点周围的随机偏移范围（格） |

### 7.3 服务器端 CaveSpawner 类设计

```cpp
class CaveSpawner {
public:
    CaveSpawner(ServerMonsterManager* monster_mgr, int level);
    
    // 更新刷新逻辑
    void update(float dt, uint64_t player_id, float player_x, float player_y,
                SendGameMsgFunc send_msg);
    
    // 初始化怪物
    void init_monsters(const CaveLevelConfig& config);
    
    // 冻结/解冻（玩家离开/进入场景）
    void freeze();
    void thaw();
    
private:
    ServerMonsterManager* monster_mgr_;
    int level_;
    
    // 刷新配置
    int max_monsters_;
    std::vector<std::string> monster_types_;
    std::vector<SpawnPoint> spawn_points_;
    float respawn_delay_;
    float spawn_check_interval_;
    float spawn_radius_;
    
    // 状态
    std::vector<uint32_t> active_monster_ids_;
    std::vector<MonsterDeathRecord> death_records_;
    float spawn_timer_;
    bool frozen_;
    
    // 尝试刷新怪物
    void try_spawn_monster(uint64_t player_id, float player_x, float player_y,
                           SendGameMsgFunc send_msg);
    
    // 选择随机刷新点
    SpawnPoint select_spawn_point(float player_x, float player_y);
};
```

### 7.4 刷新逻辑流程

```
每 spawn_check_interval 秒检查一次
       │
       ▼
计算当前存活怪物数量
       │
       ▼
如果存活数量 < max_monsters
       │
       ▼
检查死亡记录，找到已过 respawn_delay 的怪物
       │
       ▼
选择刷新点（避开玩家位置）
       │
       ▼
生成怪物，通知客户端
```

### 7.5 客户端刷新消息处理

客户端收到 `MonsterSpawnNotify` 消息后：
1. 在 `MonsterManager` 中创建新的 `MonsterSprite`
2. 播放怪物出生动画（可选）
3. 添加到渲染列表

### 7.6 场景切换时的怪物处理

**玩家离开矿洞层**：
1. 服务器调用 `freeze()` 冻结怪物状态
2. 保存怪物位置和 HP 到 `SceneState`
3. 客户端清空 `MonsterManager`

**玩家进入矿洞层**：
1. 服务器调用 `thaw()` 解冻怪物状态
2. 发送所有存活怪物的 `MonsterSpawnNotify` 给客户端
3. 客户端重新创建怪物精灵

---

## 8. 与现有系统集成

### 8.1 与场景系统集成

**GameSceneManager 扩展**：
- 支持矿洞场景类型（`is_cave: true`）
- 矿洞场景使用 `CaveSceneState` 保存状态
- 场景切换时保存/恢复怪物状态

```cpp
// 扩展 SceneState
struct SceneState {
    // ... 现有字段 ...
    bool is_cave = false;
    int cave_level = 0;
    std::vector<ServerMonster> cave_monsters;
    std::chrono::steady_clock::time_point last_spawn_time;
    bool boss_defeated = false;
};
```

### 8.2 与怪物系统集成

**ServerMonsterManager 扩展**：
- 支持多场景怪物管理
- 每个场景有独立的怪物列表
- 怪物 ID 全局唯一

```cpp
class ServerMonsterManager {
    // ... 现有方法 ...
    
    // 场景相关
    void set_scene_monsters(const std::string& scene_id, 
                           const std::vector<ServerMonster>& monsters);
    std::vector<ServerMonster> get_scene_monsters(const std::string& scene_id);
    void clear_scene_monsters(const std::string& scene_id);
    
private:
    // 按场景分组的怪物
    std::unordered_map<std::string, std::vector<uint32_t>> scene_monsters_;
};
```

### 8.3 与战斗系统集成

**CombatHandler 扩展**：
- 支持矿洞场景的战斗逻辑
- 怪物死亡时触发刷新计时器
- Boss 死亡时标记场景状态

### 8.4 与物品系统集成

**掉落物管理**：
- 矿洞怪物掉落矿石、精炼石等材料
- 掉落物在矿洞场景内（复用现有 DropItemManager）
- 玩家离开矿洞时，未拾取的掉落物消失

### 8.5 与玩家系统集成

**玩家数据扩展**：
- 记录玩家当前所在的矿洞层级
- 记录玩家矿洞探索进度（已到达最深层）
- 死亡惩罚：传回农场，丢失部分物品

```cpp
struct PlayerBizData {
    // ... 现有字段 ...
    int32_t cave_level = 0;           // 当前矿洞层级（0=不在矿洞）
    int32_t max_cave_level = 0;       // 已到达的最深层级
};
```

### 8.6 与输入系统集成

**InputManager 扩展**：
- 矿洞场景中，攻击键（E/鼠标左键）触发战斗
- 楼梯交互自动触发（无需按键）

### 8.7 与渲染系统集成

**GameRenderer 扩展**：
- 矿洞场景使用 `CaveLighting` 渲染光照
- 矿洞场景不渲染农场相关 UI（如作物状态）
- 矿洞场景渲染战斗 UI（HP 条、伤害数字）

### 8.8 消息流整合

**场景切换消息流**：
```
Client                          Server
  │                               │
  │── SceneChangeReq ────────────▶│ 验证玩家位置
  │   {target_scene}              │ 保存当前场景状态
  │                               │ 加载目标场景
  │                               │ 发送怪物数据
  │◀── SceneChangeResp ──────────│
  │   {scene_id, monsters, ...}  │
  │                               │
  │── MonsterSpawnNotify (×N) ──▶│ 每个怪物一条消息
  │                               │
```

**怪物同步消息流**：
```
Server (每 200ms)                 Client
  │                               │
  │── MonsterMoveNotify ─────────▶│ 更新怪物位置
  │   {monster_id, x, y, state}  │
  │                               │
  │── MonsterAttackNotify ───────▶│ 播放怪物攻击动画
  │   {monster_id, damage}        │ 扣减玩家HP
```

---

## 9. 实现范围

### 9.1 Phase 2 实现范围

**核心功能**：
- 矿洞入口（农场传送门）
- 层间场景切换（楼梯）
- 矿洞层配置（cave_levels.json）
- 程序化地图生成
- 矿洞光照系统（4 种效果）
- 怪物刷新机制
- 场景状态保存/恢复

**不包含在 Phase 2 中**：
- Boss 怪物（Phase 3）
- 武器合成系统（Phase 3）
- 武器强化系统（Phase 3）
- 战斗经验+升级（Phase 3）

### 9.2 新增文件

| 文件 | 职责 |
|------|------|
| `scripts/client/data/cave_levels.json` | 矿洞层配置 |
| `scripts/client/cave_manager.py` | 矿洞场景管理器 |
| `scripts/client/cave_generator.py` | 程序化地图生成器 |
| `scripts/client/cave_lighting.py` | 矿洞光照系统 |
| `scripts/server/game_server/src/cave_spawner.h` | 服务器怪物刷新器头文件 |
| `scripts/server/game_server/src/cave_spawner.cpp` | 服务器怪物刷新器实现 |

### 9.3 修改文件

| 文件 | 修改内容 |
|------|---------|
| `scripts/client/constants.py` | 添加 CAVE_PORTAL、STAIRS_UP/DOWN 类型 |
| `scripts/client/scene_defs.py` | 添加矿洞场景定义 |
| `scripts/client/game_scene.py` | 集成 CaveManager、CaveLighting |
| `scripts/client/game_renderer.py` | 矿洞场景渲染逻辑 |
| `scripts/client/network_dispatcher.py` | 场景切换消息处理 |
| `scripts/client/input_manager.py` | 矿洞场景输入处理 |
| `scripts/server/game_server/src/monster_manager.h` | 多场景怪物管理 |
| `scripts/server/game_server/src/monster_manager.cpp` | 多场景怪物管理实现 |
| `scripts/server/game_server/src/combat_handler.h` | 矿洞战斗逻辑 |
| `scripts/server/game_server/src/combat_handler.cpp` | 矿洞战斗逻辑实现 |
| `scripts/server/game_server/src/game_server.h` | CaveSpawner 集成 |
| `scripts/server/game_server/src/game_server.cpp` | 场景切换处理 |
| `scripts/server/game_server/src/scene_state.h` | 矿洞场景状态 |
| `scripts/server/game_server/CMakeLists.txt` | 添加新源文件 |
| `shared/message_ids.json` | 添加场景切换消息 ID |
| `scripts/server/common/include/message_ids.h` | 消息 ID 生成 |
| `scripts/client/message_ids.py` | 消息 ID 生成 |
| `scripts/common/proto/player.proto` | 场景切换消息定义 |
| `scripts/client/interaction.py` | 矿洞入口交互规则 |
| 农场地图文件 | 添加传送门对象 |

### 9.4 新增消息 ID

```cpp
MSG_ID_SCENE_CHANGE_REQ = 5060,   // 场景切换请求
MSG_ID_SCENE_CHANGE_RESP = 5061,  // 场景切换响应
MSG_ID_CAVE_ENTER = 5070,         // 进入矿洞
MSG_ID_CAVE_EXIT = 5071,          // 离开矿洞
MSG_ID_CAVE_LEVEL_CHANGE = 5072,  // 矿洞层级切换
```

---

## 10. 测试策略

### 10.1 单元测试

- **CaveGenerator 测试**：验证地图生成算法，检查房间不重叠、走廊连通
- **CaveLighting 测试**：验证光照渲染逻辑，检查动态效果
- **CaveSpawner 测试**：验证怪物刷新逻辑，检查延迟和数量限制

### 10.2 集成测试

- **场景切换测试**：验证农场↔矿洞、层间切换流程
- **怪物刷新测试**：验证怪物死亡后延迟刷新、场景切换冻结/解冻
- **光照渲染测试**：验证矿洞场景光照效果

### 10.3 性能测试

- **地图生成性能**：测试程序化地图生成耗时
- **光照渲染性能**：测试矿洞场景帧率
- **怪物同步性能**：测试多怪物场景下的网络同步

---

## 11. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 程序化地图生成质量不稳定 | 玩家体验差 | 添加手动调整参数，优化算法 |
| 光照渲染性能问题 | 帧率下降 | 降低更新频率，缓存遮罩 |
| 场景切换状态丢失 | 玩家进度丢失 | 完善状态保存/恢复逻辑 |
| 多场景怪物管理复杂 | Bug 增加 | 充分测试，简化设计 |
