# 怪兽与战斗系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现怪兽与战斗系统，包含怪物定义/AI、挥砍攻击、伤害计算、玩家HP/死亡、掉落奖励、矿洞场景、Boss、武器合成强化。

**Architecture:** 数据驱动架构，怪物/武器/掉落/矿洞配置全部用 JSON。客户端 MonsterManager 管理怪物实例和 AI，CombatSystem 处理攻击和伤害。服务器端验证攻击合法性，计算伤害，广播结果。复用现有 DropItemManager 处理掉落物。

**Tech Stack:** Python + PyGame (客户端), C++ + libevent + nlohmann/json (服务器), Protobuf 网络消息, JSON 数据配置

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `scripts/client/data/monster_defs.json` | 怪物基础定义（HP、攻击、速度、AI参数） |
| `scripts/client/data/weapon_defs.json` | 武器定义（攻击力、范围、速度、击退） |
| `scripts/client/data/drop_table.json` | 怪物掉落表（物品、数量、概率） |
| `scripts/client/data/cave_levels.json` | 矿洞层配置（场景、怪物、刷新点） |
| `scripts/client/data/weapon_recipes.json` | 武器合成配方 |
| `scripts/client/data/boss_defs.json` | Boss 定义（阶段、特殊攻击） |
| `scripts/client/monster_sprite.py` | 怪物精灵渲染、动画、HP条 |
| `scripts/client/monster_manager.py` | 怪物实例管理、AI状态机、刷新 |
| `scripts/client/combat_system.py` | 挥砍攻击、伤害计算、击退 |
| `scripts/client/weapon_manager.py` | 武器数据加载、装备管理 |
| `scripts/client/battle_ui.py` | 战斗UI（玩家HP条、伤害数字） |
| `scripts/client/cave_manager.py` | 矿洞场景管理、光照效果 |
| `scripts/server/game_server/src/monster_manager.h` | 服务器怪物管理器头文件 |
| `scripts/server/game_server/src/monster_manager.cpp` | 服务器怪物管理器实现 |
| `scripts/server/game_server/src/combat_handler.h` | 服务器战斗处理器头文件 |
| `scripts/server/game_server/src/combat_handler.cpp` | 服务器战斗处理器实现 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `scripts/client/constants.py` | 新增战斗相关常量、ObjectType.CAVE_ENTRANCE |
| `scripts/client/input_manager.py` | 新增攻击键（E键、鼠标左键） |
| `scripts/client/game_scene.py` | 集成 MonsterManager、CombatSystem、BattleUI |
| `scripts/client/game_renderer.py` | 渲染管线增加怪物层、战斗UI层 |
| `scripts/client/network_dispatcher.py` | 新增战斗消息处理器 |
| `scripts/client/message_ids.py` | 新增战斗消息ID |
| `scripts/client/interaction.py` | 新增 CAVE_ENTRANCE 匹配规则 |
| `scripts/client/scene/scene_defs.py` | 新增矿洞场景定义 |
| `scripts/server/common/include/message_ids.h` | 新增战斗消息ID |
| `scripts/server/game_server/src/player.h` | PlayerBizData 新增HP/战斗字段 |
| `scripts/server/game_server/src/player.cpp` | 新增字段序列化 |
| `scripts/server/game_server/src/scene_state.h` | SceneState 新增 MonsterManager |
| `scripts/server/game_server/src/game_server.h` | 新增 CombatHandler |
| `scripts/server/game_server/src/game_server.cpp` | 注册战斗消息handler、update中驱动怪物 |
| `scripts/server/game_server/src/game_scene_manager.cpp` | 支持矿洞场景 |
| `shared/message_ids.json` | 新增战斗消息ID定义 |

---

## Phase 1: 基础战斗（核心）

### Task 1: 消息ID与常量定义

**Files:**
- Modify: `shared/message_ids.json`
- Modify: `scripts/server/common/include/message_ids.h`
- Modify: `scripts/client/message_ids.py`
- Modify: `scripts/client/constants.py`

- [ ] **Step 1: 在 message_ids.json 中添加战斗消息ID**

在 `shared/message_ids.json` 的 `message_ids` 数组末尾添加：

```json
{"code": 5001, "name": "MSG_ID_ATTACK_REQ", "description": "攻击请求"},
{"code": 5002, "name": "MSG_ID_ATTACK_NOTIFY", "description": "攻击结果通知"},
{"code": 5010, "name": "MSG_ID_MONSTER_SPAWN_NOTIFY", "description": "怪物刷新通知"},
{"code": 5011, "name": "MSG_ID_MONSTER_DEATH_NOTIFY", "description": "怪物死亡通知"},
{"code": 5012, "name": "MSG_ID_MONSTER_MOVE_NOTIFY", "description": "怪物移动通知"},
{"code": 5013, "name": "MSG_ID_MONSTER_ATTACK_NOTIFY", "description": "怪物攻击通知"},
{"code": 5020, "name": "MSG_ID_PLAYER_HP_UPDATE", "description": "玩家HP更新"},
{"code": 5021, "name": "MSG_ID_PLAYER_DEATH_NOTIFY", "description": "玩家死亡通知"},
{"code": 5022, "name": "MSG_ID_PLAYER_RESPAWN", "description": "玩家复活"},
{"code": 5030, "name": "MSG_ID_COMBAT_EXP_UPDATE", "description": "战斗经验更新"},
{"code": 5031, "name": "MSG_ID_COMBAT_LEVEL_UP", "description": "战斗升级"}
```

- [ ] **Step 2: 运行代码生成脚本更新 message_ids.h 和 message_ids.py**

```bash
cd D:/mb_workspace/farm_demo
python tools/generate_message_ids.py
```

如果脚本不存在，手动更新以下文件。

- [ ] **Step 3: 手动更新 message_ids.h（如果生成脚本不存在）**

在 `scripts/server/common/include/message_ids.h` 的 `MessageIds` 枚举中，在 `MSG_ID_ACTIVE_SLOT_CHANGE = 3101` 之后添加：

```cpp
    // 战斗相关消息 (5000-5099)
    MSG_ID_ATTACK_REQ = 5001,           // 攻击请求
    MSG_ID_ATTACK_NOTIFY = 5002,        // 攻击结果通知
    MSG_ID_MONSTER_SPAWN_NOTIFY = 5010, // 怪物刷新通知
    MSG_ID_MONSTER_DEATH_NOTIFY = 5011, // 怪物死亡通知
    MSG_ID_MONSTER_MOVE_NOTIFY = 5012,  // 怪物移动通知
    MSG_ID_MONSTER_ATTACK_NOTIFY = 5013,// 怪物攻击通知
    MSG_ID_PLAYER_HP_UPDATE = 5020,     // 玩家HP更新
    MSG_ID_PLAYER_DEATH_NOTIFY = 5021,  // 玩家死亡通知
    MSG_ID_PLAYER_RESPAWN = 5022,       // 玩家复活
    MSG_ID_COMBAT_EXP_UPDATE = 5030,    // 战斗经验更新
    MSG_ID_COMBAT_LEVEL_UP = 5031       // 战斗升级
```

- [ ] **Step 4: 手动更新 message_ids.py（如果生成脚本不存在）**

在 `scripts/client/message_ids.py` 的 `MessageIds` 类中，在 `MSG_ID_ACTIVE_SLOT_CHANGE = 3101` 之后添加：

```python
    # 战斗相关消息 (5000-5099)
    MSG_ID_ATTACK_REQ = 5001  # 攻击请求
    MSG_ID_ATTACK_NOTIFY = 5002  # 攻击结果通知
    MSG_ID_MONSTER_SPAWN_NOTIFY = 5010  # 怪物刷新通知
    MSG_ID_MONSTER_DEATH_NOTIFY = 5011  # 怪物死亡通知
    MSG_ID_MONSTER_MOVE_NOTIFY = 5012  # 怪物移动通知
    MSG_ID_MONSTER_ATTACK_NOTIFY = 5013  # 怪物攻击通知
    MSG_ID_PLAYER_HP_UPDATE = 5020  # 玩家HP更新
    MSG_ID_PLAYER_DEATH_NOTIFY = 5021  # 玩家死亡通知
    MSG_ID_PLAYER_RESPAWN = 5022  # 玩家复活
    MSG_ID_COMBAT_EXP_UPDATE = 5030  # 战斗经验更新
    MSG_ID_COMBAT_LEVEL_UP = 5031  # 战斗升级
```

同时在文件顶部的 import 部分添加新的消息ID导入（如果使用了 from-import 模式）。

- [ ] **Step 5: 在 constants.py 中添加战斗常量和新 ObjectType**

在 `ObjectType` 枚举中添加（在 STOVE=14 之后）：

```python
    CAVE_ENTRANCE = 20  # 矿洞入口
    STAIRS_DOWN = 21    # 向下楼梯
    STAIRS_UP = 22      # 向上楼梯
    ANVIL = 23          # 铁砧
```

在 `OBJECT_PROPERTIES` 字典中添加：

```python
    ObjectType.CAVE_ENTRANCE: {
        "walkable": True,
        "interactable": True,
        "interact_type": "enter_cave",
    },
    ObjectType.STAIRS_DOWN: {
        "walkable": True,
        "interactable": True,
        "interact_type": "stairs_down",
    },
    ObjectType.STAIRS_UP: {
        "walkable": True,
        "interactable": True,
        "interact_type": "stairs_up",
    },
    ObjectType.ANVIL: {
        "walkable": False,
        "interactable": True,
        "interact_type": "craft",
    },
```

在文件末尾添加战斗常量：

```python
# ========== 战斗系统常量 ==========
MONSTER_SYNC_INTERVAL = 0.2        # 怪物同步间隔（秒）
MONSTER_HP_BAR_WIDTH = 24          # 怪物HP条宽度（像素）
MONSTER_HP_BAR_HEIGHT = 3          # 怪物HP条高度（像素）
MONSTER_HP_BAR_OFFSET_Y = -8       # HP条在怪物头顶偏移
MONSTER_HIT_FLASH_DURATION = 0.15  # 受击闪白持续时间（秒）
MONSTER_KNOCKBACK_DURATION = 0.2   # 击退持续时间（秒）

PLAYER_HP_BAR_WIDTH = 120          # 玩家HP条宽度
PLAYER_HP_BAR_HEIGHT = 12          # 玩家HP条高度
PLAYER_HP_BAR_MARGIN_TOP = 10      # HP条距屏幕顶部
PLAYER_INVINCIBLE_DURATION = 0.5   # 受伤无敌帧时长（秒）
PLAYER_DEATH_DROP_RATIO = 0.3      # 死亡丢失物品比例
PLAYER_RESPAWN_HP_RATIO = 0.5      # 复活HP恢复比例

DAMAGE_NUMBER_DURATION = 0.8       # 伤害数字显示时长（秒）
DAMAGE_NUMBER_RISE_SPEED = 40      # 伤害数字上升速度（像素/秒）

ATTACK_COOLDOWN_DEFAULT = 0.4      # 默认攻击冷却（秒）
```

- [ ] **Step 6: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add shared/message_ids.json scripts/server/common/include/message_ids.h scripts/client/message_ids.py scripts/client/constants.py
git commit -m "feat(combat): add message IDs and combat constants

Add combat-related message IDs (4000-4099 range) for attack, monster
sync, player HP, death, and experience. Add combat constants and new
ObjectType entries for cave entrance, stairs, and anvil."
```

---

### Task 2: 玩家HP扩展（服务器端）

**Files:**
- Modify: `scripts/server/game_server/src/player.h`
- Modify: `scripts/server/game_server/src/player.cpp`

- [ ] **Step 1: 在 PlayerBizData 中添加战斗字段**

在 `scripts/server/game_server/src/player.h` 的 `PlayerBizData` 结构体中，在 `std::string extra_data;` 之后添加：

```cpp
    int32_t max_hp = 100;        // 最大生命值
    int32_t current_hp = 100;    // 当前生命值
    int32_t attack_power = 0;    // 额外攻击力（装备加成）
    int32_t defense_power = 0;   // 额外防御力（装备加成）
    int32_t combat_exp = 0;      // 战斗经验值
    int32_t combat_level = 1;    // 战斗等级
    std::string equipped_weapon; // 装备的武器ID（JSON）
```

- [ ] **Step 2: 在 Player 类中添加字段访问方法**

在 `scripts/server/game_server/src/player.h` 的 Player 类中，在 `set_extra_data` 方法之后添加：

```cpp
    int32_t get_max_hp() const { return player_data_.max_hp; }
    void set_max_hp(int32_t max_hp);

    int32_t get_current_hp() const { return player_data_.current_hp; }
    void set_current_hp(int32_t current_hp);

    int32_t get_attack_power() const { return player_data_.attack_power; }
    void set_attack_power(int32_t attack_power);

    int32_t get_defense_power() const { return player_data_.defense_power; }
    void set_defense_power(int32_t defense_power);

    int32_t get_combat_exp() const { return player_data_.combat_exp; }
    void set_combat_exp(int32_t combat_exp);

    int32_t get_combat_level() const { return player_data_.combat_level; }
    void set_combat_level(int32_t combat_level);

    const std::string& get_equipped_weapon() const { return player_data_.equipped_weapon; }
    void set_equipped_weapon(const std::string& weapon_id);
```

- [ ] **Step 3: 在 player.cpp 中实现 setter 方法**

在 `scripts/server/game_server/src/player.cpp` 中，在现有 setter 方法之后添加：

```cpp
void Player::set_max_hp(int32_t max_hp) {
    player_data_.max_hp = max_hp;
    mark_dirty("max_hp");
}

void Player::set_current_hp(int32_t current_hp) {
    player_data_.current_hp = current_hp;
    mark_dirty("current_hp");
}

void Player::set_attack_power(int32_t attack_power) {
    player_data_.attack_power = attack_power;
    mark_dirty("attack_power");
}

void Player::set_defense_power(int32_t defense_power) {
    player_data_.defense_power = defense_power;
    mark_dirty("defense_power");
}

void Player::set_combat_exp(int32_t combat_exp) {
    player_data_.combat_exp = combat_exp;
    mark_dirty("combat_exp");
}

void Player::set_combat_level(int32_t combat_level) {
    player_data_.combat_level = combat_level;
    mark_dirty("combat_level");
}

void Player::set_equipped_weapon(const std::string& weapon_id) {
    player_data_.equipped_weapon = weapon_id;
    mark_dirty("equipped_weapon");
}
```

- [ ] **Step 4: 更新序列化方法**

在 `player.cpp` 的 `get_field_json` 方法中添加新字段的序列化（找到现有的字段处理逻辑，添加以下分支）：

```cpp
    if (field == "max_hp") return std::to_string(player_data_.max_hp);
    if (field == "current_hp") return std::to_string(player_data_.current_hp);
    if (field == "attack_power") return std::to_string(player_data_.attack_power);
    if (field == "defense_power") return std::to_string(player_data_.defense_power);
    if (field == "combat_exp") return std::to_string(player_data_.combat_exp);
    if (field == "combat_level") return std::to_string(player_data_.combat_level);
    if (field == "equipped_weapon") return "\"" + player_data_.equipped_weapon + "\"";
```

在 `get_all_data_json` 方法中添加新字段到 JSON 输出：

```cpp
    result += ",\"max_hp\":" + std::to_string(player_data_.max_hp);
    result += ",\"current_hp\":" + std::to_string(player_data_.current_hp);
    result += ",\"attack_power\":" + std::to_string(player_data_.attack_power);
    result += ",\"defense_power\":" + std::to_string(player_data_.defense_power);
    result += ",\"combat_exp\":" + std::to_string(player_data_.combat_exp);
    result += ",\"combat_level\":" + std::to_string(player_data_.combat_level);
    result += ",\"equipped_weapon\":\"" + player_data_.equipped_weapon + "\"";
```

- [ ] **Step 5: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 6: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(combat): extend PlayerBizData with HP and combat fields

Add max_hp, current_hp, attack_power, defense_power, combat_exp,
combat_level, and equipped_weapon to player data with serialization."
```

---

### Task 3: 怪物数据文件与加载器（客户端）

**Files:**
- Create: `scripts/client/data/monster_defs.json`
- Create: `scripts/client/data/weapon_defs.json`
- Create: `scripts/client/data/drop_table.json`
- Create: `scripts/client/weapon_manager.py`

- [ ] **Step 1: 创建 monster_defs.json**

创建目录 `scripts/client/data/`（如果不存在），然后创建文件：

```json
{
  "monsters": [
    {
      "id": "slime",
      "name": "史莱姆",
      "sprite_sheet": "monster_slime.png",
      "hp": 30,
      "attack": 5,
      "defense": 0,
      "speed": 30.0,
      "exp_reward": 10,
      "aggro_range": 5,
      "attack_range": 1,
      "attack_cooldown": 1.5,
      "wander_interval": [2.0, 5.0],
      "wander_radius": 3,
      "color": [50, 180, 50],
      "size": [12, 12]
    },
    {
      "id": "bat",
      "name": "蝙蝠",
      "sprite_sheet": "monster_bat.png",
      "hp": 20,
      "attack": 8,
      "defense": 0,
      "speed": 50.0,
      "exp_reward": 15,
      "aggro_range": 6,
      "attack_range": 1,
      "attack_cooldown": 1.0,
      "wander_interval": [1.0, 3.0],
      "wander_radius": 5,
      "color": [100, 60, 120],
      "size": [10, 10]
    },
    {
      "id": "skeleton",
      "name": "骷髅战士",
      "sprite_sheet": "monster_skeleton.png",
      "hp": 80,
      "attack": 15,
      "defense": 5,
      "speed": 25.0,
      "exp_reward": 50,
      "aggro_range": 4,
      "attack_range": 1,
      "attack_cooldown": 2.0,
      "wander_interval": [3.0, 6.0],
      "wander_radius": 2,
      "color": [200, 200, 180],
      "size": [12, 14]
    }
  ]
}
```

- [ ] **Step 2: 创建 weapon_defs.json**

```json
{
  "weapons": [
    {
      "id": "wooden_sword",
      "name": "木剑",
      "item_id": 100,
      "attack": 10,
      "attack_range": 1.5,
      "attack_speed": 0.4,
      "knockback": 2.0,
      "description": "基础的木质短剑"
    },
    {
      "id": "iron_sword",
      "name": "铁剑",
      "item_id": 101,
      "attack": 25,
      "attack_range": 1.8,
      "attack_speed": 0.35,
      "knockback": 3.0,
      "description": "坚固的铁制长剑"
    },
    {
      "id": "gold_sword",
      "name": "金剑",
      "item_id": 102,
      "attack": 40,
      "attack_range": 2.0,
      "attack_speed": 0.3,
      "knockback": 4.0,
      "description": "闪耀的黄金之剑"
    }
  ]
}
```

- [ ] **Step 3: 创建 drop_table.json**

```json
{
  "drop_tables": {
    "slime": [
      {"item_id": 201, "count": [1, 3], "chance": 0.8},
      {"item_id": 1, "count": [5, 15], "chance": 1.0},
      {"item_id": 210, "count": [1, 1], "chance": 0.15}
    ],
    "bat": [
      {"item_id": 202, "count": [1, 2], "chance": 0.7},
      {"item_id": 1, "count": [8, 20], "chance": 1.0},
      {"item_id": 210, "count": [1, 1], "chance": 0.2}
    ],
    "skeleton": [
      {"item_id": 203, "count": [1, 3], "chance": 0.9},
      {"item_id": 204, "count": [1, 2], "chance": 0.4},
      {"item_id": 1, "count": [20, 50], "chance": 1.0},
      {"item_id": 211, "count": [1, 1], "chance": 0.1},
      {"item_id": 101, "count": [1, 1], "chance": 0.05}
    ]
  }
}
```

- [ ] **Step 4: 创建 weapon_manager.py**

```python
"""
武器管理器模块
加载武器定义，管理武器装备状态。
"""
import json
import os
import logging
from typing import Dict, Optional, Any, List

logger = logging.getLogger("client.weapon_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class WeaponManager:
    """武器管理器"""

    def __init__(self):
        self._weapons: Dict[str, Dict[str, Any]] = {}
        self._item_to_weapon: Dict[int, str] = {}  # item_id -> weapon_id
        self._load_weapons()

    def _load_weapons(self):
        """加载武器定义"""
        path = os.path.join(DATA_DIR, "weapon_defs.json")
        if not os.path.exists(path):
            logger.warning(f"[WeaponManager]weapon_defs.json not found at {path}")
            return

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for w in data.get("weapons", []):
            self._weapons[w["id"]] = w
            if "item_id" in w:
                self._item_to_weapon[w["item_id"]] = w["id"]

        logger.info(f"[WeaponManager]Loaded {len(self._weapons)} weapons")

    def get_weapon(self, weapon_id: str) -> Optional[Dict[str, Any]]:
        return self._weapons.get(weapon_id)

    def get_weapon_by_item(self, item_id: int) -> Optional[Dict[str, Any]]:
        weapon_id = self._item_to_weapon.get(item_id)
        if weapon_id:
            return self._weapons.get(weapon_id)
        return None

    def is_weapon(self, item_id: int) -> bool:
        return item_id in self._item_to_weapon

    def get_all_weapons(self) -> List[Dict[str, Any]]:
        return list(self._weapons.values())
```

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/data/monster_defs.json scripts/client/data/weapon_defs.json scripts/client/data/drop_table.json scripts/client/weapon_manager.py
git commit -m "feat(combat): add monster/weapon/drop data files and WeaponManager

Add JSON config files for monster definitions, weapon definitions, and
drop tables. Add WeaponManager for loading and querying weapon data."
```

---

### Task 4: 怪物精灵与管理器（客户端）

**Files:**
- Create: `scripts/client/monster_sprite.py`
- Create: `scripts/client/monster_manager.py`

- [ ] **Step 1: 创建 monster_sprite.py**

```python
"""
怪物精灵模块
渲染怪物实体，支持动画、HP条、受击闪白效果。
"""
import math
import time
import logging
from typing import Optional, Dict, Any, Tuple

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR, Direction

logger = logging.getLogger("client.monster_sprite")


class MonsterState:
    """怪物状态常量"""
    IDLE = "idle"
    WANDER = "wander"
    CHASE = "chase"
    ATTACK = "attack"
    HIT = "hit"
    KNOCKBACK = "knockback"
    DEAD = "dead"


class MonsterSprite:
    """
    怪物精灵
    渲染单个怪物实体，支持动画、HP条、受击闪白。
    """

    def __init__(self, monster_id: int, monster_type: str, x: float, y: float,
                 hp: int, max_hp: int, config: Dict[str, Any]):
        self.monster_id = monster_id
        self.monster_type = monster_type
        self.world_x = x  # 世界坐标（像素）
        self.world_y = y
        self.hp = hp
        self.max_hp = max_hp
        self.config = config

        self.state = MonsterState.IDLE
        self.direction = Direction.DOWN

        # 受击闪白
        self._hit_flash_until = 0.0

        # 击退
        self._knockback_until = 0.0
        self._knockback_dx = 0.0
        self._knockback_dy = 0.0

        # 动画
        self._anim_frame = 0
        self._anim_timer = 0.0

        # 颜色渲染（临时方案，后续替换为精灵图）
        color = config.get("color", [128, 128, 128])
        self._color = tuple(color)
        size = config.get("size", [12, 12])
        self._width = size[0]
        self._height = size[1]

    @property
    def tile_x(self) -> int:
        return int(self.world_x / TILE_SIZE)

    @property
    def tile_y(self):
        return int(self.world_y / TILE_SIZE)

    @property
    def is_hit(self) -> bool:
        return time.time() < self._hit_flash_until

    @property
    def is_knockback(self) -> bool:
        return time.time() < self._knockback_until

    def apply_hit(self, knockback_dx: float = 0.0, knockback_dy: float = 0.0):
        """应用受击效果"""
        self._hit_flash_until = time.time() + 0.15
        if knockback_dx != 0 or knockback_dy != 0:
            self._knockback_until = time.time() + 0.2
            self._knockback_dx = knockback_dx
            self._knockback_dy = knockback_dy
        self.state = MonsterState.HIT

    def update(self, dt: float):
        """更新怪物状态"""
        now = time.time()

        # 击退移动
        if self.is_knockback:
            self.world_x += self._knockback_dx * dt * 5 * TILE_SIZE
            self.world_y += self._knockback_dy * dt * 5 * TILE_SIZE

        # 动画更新
        self._anim_timer += dt
        if self._anim_timer >= 0.2:
            self._anim_timer = 0.0
            self._anim_frame = (self._anim_frame + 1) % 4

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float):
        """
        渲染怪物到屏幕

        Args:
            screen: PyGame 屏幕 Surface
            camera_x: 相机 X 偏移（世界像素）
            camera_y: 相机 Y 偏移（世界像素）
        """
        # 计算屏幕坐标
        screen_x = (self.world_x - camera_x) * ZOOM_FACTOR
        screen_y = (self.world_y - camera_y) * ZOOM_FACTOR

        # 怪物尺寸（缩放后）
        w = self._width * ZOOM_FACTOR
        h = self._height * ZOOM_FACTOR

        # 受击闪白效果
        color = (255, 255, 255) if self.is_hit else self._color

        # 绘制怪物矩形（临时方案）
        rect = pygame.Rect(
            int(screen_x - w // 2),
            int(screen_y - h // 2),
            w, h
        )
        pygame.draw.rect(screen, color, rect)
        pygame.draw.rect(screen, (0, 0, 0), rect, 1)

        # 绘制眼睛（朝向指示）
        eye_size = max(2, ZOOM_FACTOR)
        if self.direction == Direction.DOWN:
            eye_y = int(screen_y - h // 4)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x - w // 4), eye_y), eye_size)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x + w // 4), eye_y), eye_size)
        elif self.direction == Direction.UP:
            eye_y = int(screen_y + h // 4)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x - w // 4), eye_y), eye_size)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x + w // 4), eye_y), eye_size)
        elif self.direction == Direction.LEFT:
            eye_x = int(screen_x - w // 4)
            pygame.draw.circle(screen, (0, 0, 0), (eye_x, int(screen_y)), eye_size)
        else:
            eye_x = int(screen_x + w // 4)
            pygame.draw.circle(screen, (0, 0, 0), (eye_x, int(screen_y)), eye_size)

        # 绘制 HP 条（受伤时显示）
        if self.hp < self.max_hp and self.hp > 0:
            self._render_hp_bar(screen, int(screen_x), int(screen_y - h // 2 - 6))

    def _render_hp_bar(self, screen: pygame.Surface, x: int, y: int):
        """绘制 HP 条"""
        bar_w = 24 * ZOOM_FACTOR // 2
        bar_h = 3 * ZOOM_FACTOR // 2

        # 背景
        bg_rect = pygame.Rect(x - bar_w // 2, y, bar_w, bar_h)
        pygame.draw.rect(screen, (60, 60, 60), bg_rect)

        # HP
        ratio = max(0, self.hp / self.max_hp)
        hp_rect = pygame.Rect(x - bar_w // 2, y, int(bar_w * ratio), bar_h)
        hp_color = (50, 200, 50) if ratio > 0.5 else (200, 200, 50) if ratio > 0.25 else (200, 50, 50)
        pygame.draw.rect(screen, hp_color, hp_rect)
```

- [ ] **Step 2: 创建 monster_manager.py**

```python
"""
怪物管理器模块
管理怪物实例，加载怪物定义，驱动怪物AI状态机。
处理服务器同步消息。
"""
import json
import os
import time
import math
import random
import logging
from typing import Dict, Optional, List, Any, Tuple

from .constants import TILE_SIZE, Direction
from .monster_sprite import MonsterSprite, MonsterState

logger = logging.getLogger("client.monster_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class MonsterManager:
    """
    怪物管理器
    管理场景中的所有怪物实例。
    """

    def __init__(self):
        self._monsters: Dict[int, MonsterSprite] = {}
        self._monster_defs: Dict[str, Dict[str, Any]] = {}
        self._load_monster_defs()

    def _load_monster_defs(self):
        """加载怪物定义"""
        path = os.path.join(DATA_DIR, "monster_defs.json")
        if not os.path.exists(path):
            logger.warning(f"[MonsterManager]monster_defs.json not found at {path}")
            return

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for m in data.get("monsters", []):
            self._monster_defs[m["id"]] = m

        logger.info(f"[MonsterManager]Loaded {len(self._monster_defs)} monster types")

    def get_monster_def(self, monster_type: str) -> Optional[Dict[str, Any]]:
        return self._monster_defs.get(monster_type)

    def spawn_monster(self, monster_id: int, monster_type: str,
                      x: float, y: float, hp: int, max_hp: int) -> Optional[MonsterSprite]:
        """生成怪物"""
        config = self._monster_defs.get(monster_type)
        if not config:
            logger.warning(f"[MonsterManager]Unknown monster type: {monster_type}")
            return None

        monster = MonsterSprite(monster_id, monster_type, x, y, hp, max_hp, config)
        self._monsters[monster_id] = monster
        logger.debug(f"[MonsterManager]Spawned monster_id={monster_id}, type={monster_type}")
        return monster

    def remove_monster(self, monster_id: int):
        """移除怪物"""
        if monster_id in self._monsters:
            del self._monsters[monster_id]
            logger.debug(f"[MonsterManager]Removed monster_id={monster_id}")

    def get_monster(self, monster_id: int) -> Optional[MonsterSprite]:
        return self._monsters.get(monster_id)

    def get_all_monsters(self) -> Dict[int, MonsterSprite]:
        return self._monsters

    def get_monsters_in_range(self, world_x: float, world_y: float,
                              range_tiles: float) -> List[MonsterSprite]:
        """获取范围内的怪物"""
        result = []
        range_px = range_tiles * TILE_SIZE
        for m in self._monsters.values():
            if m.hp <= 0:
                continue
            dx = m.world_x - world_x
            dy = m.world_y - world_y
            dist = math.sqrt(dx * dx + dy * dy)
            if dist <= range_px:
                result.append(m)
        return result

    def update(self, dt: float, player_x: float, player_y: float):
        """
        更新所有怪物（客户端AI预测）

        Args:
            dt: 帧间隔
            player_x: 玩家世界坐标X
            player_y: 玩家世界坐标Y
        """
        for monster in self._monsters.values():
            if monster.hp <= 0:
                continue
            self._update_monster_ai(monster, dt, player_x, player_y)
            monster.update(dt)

    def _update_monster_ai(self, monster: MonsterSprite, dt: float,
                           player_x: float, player_y: float):
        """更新怪物AI（客户端预测）"""
        config = monster.config
        aggro_range = config.get("aggro_range", 5) * TILE_SIZE
        speed = config.get("speed", 30.0)

        dx = player_x - monster.world_x
        dy = player_y - monster.world_y
        dist = math.sqrt(dx * dx + dy * dy)

        if monster.is_knockback:
            return

        if dist < aggro_range and dist > TILE_SIZE * 0.5:
            # 追击玩家
            monster.state = MonsterState.CHASE
            if dist > 0:
                nx = dx / dist
                ny = dy / dist
                monster.world_x += nx * speed * dt
                monster.world_y += ny * speed * dt

                # 更新朝向
                if abs(dx) > abs(dy):
                    monster.direction = Direction.RIGHT if dx > 0 else Direction.LEFT
                else:
                    monster.direction = Direction.DOWN if dy > 0 else Direction.UP
        else:
            # 游荡
            if monster.state != MonsterState.WANDER:
                monster.state = MonsterState.IDLE

    def handle_monster_spawn(self, monster_id: int, monster_type: str,
                             x: float, y: float, hp: int, max_hp: int):
        """处理怪物刷新消息"""
        self.spawn_monster(monster_id, monster_type, x, y, hp, max_hp)

    def handle_monster_death(self, monster_id: int):
        """处理怪物死亡消息"""
        monster = self._monsters.get(monster_id)
        if monster:
            monster.hp = 0
            # 延迟移除（播放死亡动画）
            # 简单方案：立即移除
            self.remove_monster(monster_id)

    def handle_monster_move(self, monster_id: int, x: float, y: float, state: str):
        """处理怪物移动同步消息"""
        monster = self._monsters.get(monster_id)
        if monster:
            # 插值到目标位置
            monster.world_x = x
            monster.world_y = y
            if state:
                monster.state = state

    def handle_monster_attack(self, monster_id: int, damage: int):
        """处理怪物攻击通知"""
        monster = self._monsters.get(monster_id)
        if monster:
            monster.state = MonsterState.ATTACK

    def clear(self):
        """清空所有怪物（场景切换时）"""
        self._monsters.clear()
        logger.info("[MonsterManager]Cleared all monsters")

    @property
    def monster_count(self) -> int:
        return len(self._monsters)
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/monster_sprite.py scripts/client/monster_manager.py
git commit -m "feat(combat): add MonsterSprite and MonsterManager

Add monster rendering with color rectangles, HP bars, hit flash effect.
Add MonsterManager for spawning, AI updates (wander+chase), and
handling server sync messages."
```

---

### Task 5: 战斗系统与武器管理（客户端）

**Files:**
- Create: `scripts/client/combat_system.py`
- Create: `scripts/client/battle_ui.py`

- [ ] **Step 1: 创建 combat_system.py**

```python
"""
战斗系统模块
处理玩家挥砍攻击、伤害计算、击退效果。
"""
import math
import time
import random
import logging
from typing import Optional, Dict, Any, List, Tuple

from .constants import TILE_SIZE, Direction, ATTACK_COOLDOWN_DEFAULT
from .monster_manager import MonsterManager
from .monster_sprite import MonsterSprite
from .weapon_manager import WeaponManager

logger = logging.getLogger("client.combat_system")


class CombatSystem:
    """
    战斗系统
    处理攻击判定、伤害计算、击退效果。
    """

    def __init__(self, weapon_manager: WeaponManager, monster_manager: MonsterManager):
        self._weapon_mgr = weapon_manager
        self._monster_mgr = monster_manager

        # 攻击冷却
        self._last_attack_time = 0.0

        # 当前装备的武器
        self._equipped_weapon_id: Optional[str] = None

    @property
    def equipped_weapon(self) -> Optional[Dict[str, Any]]:
        if self._equipped_weapon_id:
            return self._weapon_mgr.get_weapon(self._equipped_weapon_id)
        return None

    def equip_weapon(self, weapon_id: Optional[str]):
        """装备武器"""
        self._equipped_weapon_id = weapon_id
        logger.info(f"[Combat]Equipped weapon: {weapon_id}")

    def equip_weapon_by_item(self, item_id: int):
        """通过物品ID装备武器"""
        weapon = self._weapon_mgr.get_weapon_by_item(item_id)
        if weapon:
            self._equipped_weapon_id = weapon["id"]
            logger.info(f"[Combat]Equipped weapon by item: {item_id} -> {weapon['id']}")
        else:
            self._equipped_weapon_id = None

    def can_attack(self) -> bool:
        """是否可以攻击"""
        weapon = self.equipped_weapon
        if not weapon:
            return False

        now = time.time()
        cooldown = weapon.get("attack_speed", ATTACK_COOLDOWN_DEFAULT)
        return (now - self._last_attack_time) >= cooldown

    def try_attack(self, player_x: float, player_y: float,
                   direction: str, player_attack_power: int = 0
                   ) -> List[Tuple[int, int, bool]]:
        """
        尝试攻击

        Args:
            player_x: 玩家世界坐标X（像素）
            player_y: 玩家世界坐标Y（像素）
            direction: 攻击方向
            player_attack_power: 玩家额外攻击力

        Returns:
            [(monster_id, damage, is_kill), ...] 命中的怪物列表
        """
        if not self.can_attack():
            return []

        weapon = self.equipped_weapon
        if not weapon:
            return []

        self._last_attack_time = time.time()

        # 计算攻击方向向量
        dir_vectors = {
            Direction.UP: (0, -1),
            Direction.DOWN: (0, 1),
            Direction.LEFT: (-1, 0),
            Direction.RIGHT: (1, 0),
        }
        dir_vec = dir_vectors.get(direction, (0, 1))

        # 攻击范围（像素）
        attack_range = weapon.get("attack_range", 1.5) * TILE_SIZE
        weapon_attack = weapon.get("attack", 10)
        knockback = weapon.get("knockback", 2.0)

        # 攻击判定区域（面向方向的矩形）
        # 攻击中心点在玩家前方
        attack_cx = player_x + dir_vec[0] * attack_range * 0.6
        attack_cy = player_y + dir_vec[1] * attack_range * 0.6

        # 检测范围内怪物
        hits = []
        for monster in self._monster_mgr.get_all_monsters().values():
            if monster.hp <= 0:
                continue

            dx = monster.world_x - attack_cx
            dy = monster.world_y - attack_cy
            dist = math.sqrt(dx * dx + dy * dy)

            if dist <= attack_range:
                # 计算伤害
                monster_def = monster.config.get("defense", 0)
                base_damage = weapon_attack + player_attack_power - monster_def
                random_factor = random.uniform(0.9, 1.1)
                damage = max(1, int(base_damage * random_factor))

                # 应用伤害
                monster.hp = max(0, monster.hp - damage)

                # 计算击退方向
                knockback_dist = knockback * TILE_SIZE
                if dist > 0:
                    kb_dx = (dx / dist) * knockback_dist * 0.3
                    kb_dy = (dy / dist) * knockback_dist * 0.3
                else:
                    kb_dx = dir_vec[0] * knockback_dist * 0.3
                    kb_dy = dir_vec[1] * knockback_dist * 0.3

                monster.apply_hit(kb_dx, kb_dy)

                is_kill = monster.hp <= 0
                hits.append((monster.monster_id, damage, is_kill))

                logger.info(f"[Combat]Hit monster_id={monster.monster_id}, "
                           f"damage={damage}, hp={monster.hp}, kill={is_kill}")

        return hits

    def get_attack_info(self) -> Optional[Dict[str, Any]]:
        """获取当前攻击信息（用于发送给服务器）"""
        weapon = self.equipped_weapon
        if not weapon:
            return None
        return {
            "weapon_id": weapon["id"],
            "attack": weapon.get("attack", 10),
            "attack_range": weapon.get("attack_range", 1.5),
            "knockback": weapon.get("knockback", 2.0),
        }
```

- [ ] **Step 2: 创建 battle_ui.py**

```python
"""
战斗UI模块
渲染玩家HP条、伤害数字。
"""
import time
import logging
from typing import List, Tuple

import pygame

from .constants import (
    PLAYER_HP_BAR_WIDTH, PLAYER_HP_BAR_HEIGHT,
    PLAYER_HP_BAR_MARGIN_TOP, DAMAGE_NUMBER_DURATION,
    DAMAGE_NUMBER_RISE_SPEED, ZOOM_FACTOR,
)

logger = logging.getLogger("client.battle_ui")


class DamageNumber:
    """浮动伤害数字"""

    def __init__(self, x: float, y: float, damage: int, is_heal: bool = False):
        self.x = x
        self.y = y
        self.damage = damage
        self.is_heal = is_heal
        self.spawn_time = time.time()

    @property
    def is_expired(self) -> bool:
        return (time.time() - self.spawn_time) >= DAMAGE_NUMBER_DURATION

    def update(self, dt: float):
        self.y -= DAMAGE_NUMBER_RISE_SPEED * dt


class BattleUI:
    """战斗UI渲染器"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 伤害数字列表
        self._damage_numbers: List[DamageNumber] = []

        # 字体
        self._font = pygame.font.SysFont("arial", 14, bold=True)
        self._hp_font = pygame.font.SysFont("arial", 10)

    def add_damage_number(self, world_x: float, world_y: float, damage: int,
                          is_heal: bool = False):
        """添加伤害数字"""
        self._damage_numbers.append(
            DamageNumber(world_x, world_y, damage, is_heal)
        )

    def update(self, dt: float):
        """更新伤害数字"""
        for dn in self._damage_numbers:
            dn.update(dt)
        self._damage_numbers = [dn for dn in self._damage_numbers if not dn.is_expired]

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float,
               player_hp: int, player_max_hp: int):
        """
        渲染战斗UI

        Args:
            screen: PyGame 屏幕
            camera_x: 相机X偏移
            camera_y: 相机Y偏移
            player_hp: 玩家当前HP
            player_max_hp: 玩家最大HP
        """
        # 玩家HP条（左上角）
        self._render_player_hp(screen, player_hp, player_max_hp)

        # 伤害数字
        for dn in self._damage_numbers:
            self._render_damage_number(screen, dn, camera_x, camera_y)

    def _render_player_hp(self, screen: pygame.Surface, hp: int, max_hp: int):
        """绘制玩家HP条"""
        x = 10
        y = PLAYER_HP_BAR_MARGIN_TOP + 20  # 在时间HUD下方

        # 标签
        label = self._hp_font.render("HP", True, (200, 200, 200))
        screen.blit(label, (x, y - 12))

        # 背景
        bg_rect = pygame.Rect(x, y, PLAYER_HP_BAR_WIDTH, PLAYER_HP_BAR_HEIGHT)
        pygame.draw.rect(screen, (60, 60, 60), bg_rect)
        pygame.draw.rect(screen, (100, 100, 100), bg_rect, 1)

        # HP
        ratio = max(0, hp / max_hp) if max_hp > 0 else 0
        hp_rect = pygame.Rect(x, y, int(PLAYER_HP_BAR_WIDTH * ratio), PLAYER_HP_BAR_HEIGHT)
        hp_color = (50, 200, 50) if ratio > 0.5 else (200, 200, 50) if ratio > 0.25 else (200, 50, 50)
        pygame.draw.rect(screen, hp_color, hp_rect)

        # 数值
        hp_text = self._hp_font.render(f"{hp}/{max_hp}", True, (255, 255, 255))
        screen.blit(hp_text, (x + PLAYER_HP_BAR_WIDTH + 5, y - 2))

    def _render_damage_number(self, screen: pygame.Surface, dn: DamageNumber,
                              camera_x: float, camera_y: float):
        """绘制伤害数字"""
        screen_x = (dn.x - camera_x) * ZOOM_FACTOR
        screen_y = (dn.y - camera_y) * ZOOM_FACTOR

        if dn.is_heal:
            color = (50, 255, 50)
            text = f"+{dn.damage}"
        else:
            color = (255, 50, 50)
            text = f"-{dn.damage}"

        # 淡出效果
        elapsed = time.time() - dn.spawn_time
        alpha = max(0, 255 - int(255 * elapsed / DAMAGE_NUMBER_DURATION))

        surf = self._font.render(text, True, color)
        if alpha < 255:
            surf.set_alpha(alpha)

        rect = surf.get_rect(center=(int(screen_x), int(screen_y)))
        screen.blit(surf, rect)
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/combat_system.py scripts/client/battle_ui.py
git commit -m "feat(combat): add CombatSystem and BattleUI

Add swing attack with directional hit detection, damage formula with
random variance, knockback effect. Add BattleUI with player HP bar
and floating damage numbers."
```

---

### Task 6: 输入管理器扩展（客户端）

**Files:**
- Modify: `scripts/client/input_manager.py`

- [ ] **Step 1: 添加攻击动作到 InputManager**

在 `scripts/client/input_manager.py` 的 `InputManager.__init__` 方法中，在 `self._action_map` 字典中添加：

```python
            "attack": [pygame.K_e],
```

- [ ] **Step 2: 添加鼠标攻击检测方法**

在 `InputManager` 类中添加方法：

```python
    def is_attack_pressed(self) -> bool:
        """
        检查攻击键是否被按下（E键或鼠标左键）

        Returns:
            True 表示攻击键被按下
        """
        return self.is_action_pressed("attack") or self.is_left_mouse_just_pressed()

    def get_attack_direction(self, player_screen_x: int, player_screen_y: int) -> str:
        """
        根据鼠标位置计算攻击方向

        Args:
            player_screen_x: 玩家屏幕坐标X
            player_screen_y: 玩家屏幕坐标Y

        Returns:
            方向字符串 ("up", "down", "left", "right")
        """
        mx, my = self.mouse_pos
        dx = mx - player_screen_x
        dy = my - player_screen_y

        if abs(dx) > abs(dy):
            return "right" if dx > 0 else "left"
        else:
            return "down" if dy > 0 else "up"
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/input_manager.py
git commit -m "feat(combat): add attack input handling

Add attack action (E key) and mouse left click detection. Add
get_attack_direction() for computing swing direction from mouse."
```

---

### Task 7: 服务器战斗处理器

**Files:**
- Create: `scripts/server/game_server/src/monster_manager.h`
- Create: `scripts/server/game_server/src/monster_manager.cpp`
- Create: `scripts/server/game_server/src/combat_handler.h`
- Create: `scripts/server/game_server/src/combat_handler.cpp`
- Modify: `scripts/server/game_server/src/CMakeLists.txt`

- [ ] **Step 1: 创建 monster_manager.h（服务器端）**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <ctime>

namespace farm {

struct MonsterDef {
    std::string id;
    std::string name;
    int32_t hp;
    int32_t attack;
    int32_t defense;
    float speed;
    int32_t exp_reward;
    float aggro_range;
    float attack_range;
    float attack_cooldown;
};

struct ServerMonster {
    uint32_t monster_id;
    std::string monster_type;
    float x;                // 世界坐标（像素）
    float y;
    int32_t hp;
    int32_t max_hp;
    int32_t attack;
    int32_t defense;
    float speed;
    int32_t exp_reward;
    time_t last_attack_time;
    time_t spawn_time;
};

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

class ServerMonsterManager {
public:
    ServerMonsterManager();

    // 加载怪物定义
    void load_definitions(const std::string& json_path);

    // 生成怪物
    uint32_t spawn_monster(const std::string& monster_type, float x, float y);

    // 移除怪物
    void remove_monster(uint32_t monster_id);

    // 获取怪物
    ServerMonster* get_monster(uint32_t monster_id);
    const ServerMonster* get_monster(uint32_t monster_id) const;

    // 获取范围内怪物
    std::vector<ServerMonster*> get_monsters_in_range(float x, float y, float range);

    // 更新所有怪物（AI、攻击）
    void update(float dt, uint64_t player_id, float player_x, float player_y,
                SendGameMsgFunc send_msg);

    // 对怪物造成伤害
    int32_t damage_monster(uint32_t monster_id, int32_t damage);

    // 怪物是否死亡
    bool is_monster_dead(uint32_t monster_id) const;

    // 获取怪物数量
    size_t monster_count() const { return monsters_.size(); }

private:
    std::unordered_map<std::string, MonsterDef> monster_defs_;
    std::unordered_map<uint32_t, ServerMonster> monsters_;
    uint32_t next_monster_id_ = 1;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace farm
```

- [ ] **Step 2: 创建 monster_manager.cpp**

```cpp
#include "monster_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

namespace farm {

ServerMonsterManager::ServerMonsterManager() = default;

void ServerMonsterManager::load_definitions(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        SPDLOG_WARN("[MonsterManager]Cannot open monster defs: {}", json_path);
        return;
    }

    nlohmann::json j;
    f >> j;

    for (const auto& m : j["monsters"]) {
        MonsterDef def;
        def.id = m["id"];
        def.name = m["name"];
        def.hp = m["hp"];
        def.attack = m["attack"];
        def.defense = m["defense"];
        def.speed = m["speed"];
        def.exp_reward = m["exp_reward"];
        def.aggro_range = m["aggro_range"];
        def.attack_range = m["attack_range"];
        def.attack_cooldown = m["attack_cooldown"];
        monster_defs_[def.id] = def;
    }

    SPDLOG_INFO("[MonsterManager]Loaded {} monster definitions", monster_defs_.size());
}

uint32_t ServerMonsterManager::spawn_monster(const std::string& monster_type, float x, float y) {
    auto it = monster_defs_.find(monster_type);
    if (it == monster_defs_.end()) {
        SPDLOG_WARN("[MonsterManager]Unknown monster type: {}", monster_type);
        return 0;
    }

    const auto& def = it->second;
    uint32_t id = next_monster_id_++;

    ServerMonster monster;
    monster.monster_id = id;
    monster.monster_type = monster_type;
    monster.x = x;
    monster.y = y;
    monster.hp = def.hp;
    monster.max_hp = def.hp;
    monster.attack = def.attack;
    monster.defense = def.defense;
    monster.speed = def.speed;
    monster.exp_reward = def.exp_reward;
    monster.last_attack_time = 0;
    monster.spawn_time = std::time(nullptr);

    monsters_[id] = monster;
    SPDLOG_INFO("[MonsterManager]Spawned monster_id={} type={} at ({},{})", id, monster_type, x, y);
    return id;
}

void ServerMonsterManager::remove_monster(uint32_t monster_id) {
    monsters_.erase(monster_id);
}

ServerMonster* ServerMonsterManager::get_monster(uint32_t monster_id) {
    auto it = monsters_.find(monster_id);
    return it != monsters_.end() ? &it->second : nullptr;
}

const ServerMonster* ServerMonsterManager::get_monster(uint32_t monster_id) const {
    auto it = monsters_.find(monster_id);
    return it != monsters_.end() ? &it->second : nullptr;
}

std::vector<ServerMonster*> ServerMonsterManager::get_monsters_in_range(float x, float y, float range) {
    std::vector<ServerMonster*> result;
    for (auto& [id, m] : monsters_) {
        float dx = m.x - x;
        float dy = m.y - y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= range) {
            result.push_back(&m);
        }
    }
    return result;
}

void ServerMonsterManager::update(float dt, uint64_t player_id,
                                   float player_x, float player_y,
                                   SendGameMsgFunc send_msg) {
    time_t now = std::time(nullptr);

    for (auto& [id, m] : monsters_) {
        if (m.hp <= 0) continue;

        float dx = player_x - m.x;
        float dy = player_y - m.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        auto def_it = monster_defs_.find(m.monster_type);
        if (def_it == monster_defs_.end()) continue;

        const auto& def = def_it->second;
        float aggro_range = def.aggro_range * 16.0f;  // TILE_SIZE = 16
        float attack_range = def.attack_range * 16.0f;

        if (dist < aggro_range) {
            // 追击
            if (dist > attack_range && dist > 0) {
                float nx = dx / dist;
                float ny = dy / dist;
                m.x += nx * m.speed * dt;
                m.y += ny * m.speed * dt;
            }
            // 攻击
            else if (dist <= attack_range) {
                float cooldown = def.attack_cooldown;
                if (difftime(now, m.last_attack_time) >= cooldown) {
                    m.last_attack_time = now;
                    // 通知客户端怪物攻击（伤害由客户端计算展示，服务器验证）
                    // 这里简化：服务器直接通知
                }
            }
        }
    }
}

int32_t ServerMonsterManager::damage_monster(uint32_t monster_id, int32_t damage) {
    auto* m = get_monster(monster_id);
    if (!m || m->hp <= 0) return 0;

    m->hp -= damage;
    if (m->hp < 0) m->hp = 0;

    return m->hp;
}

bool ServerMonsterManager::is_monster_dead(uint32_t monster_id) const {
    const auto* m = get_monster(monster_id);
    return m && m->hp <= 0;
}

}  // namespace farm
```

- [ ] **Step 3: 创建 combat_handler.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace farm {

class Player;
class ServerMonsterManager;
class DropItemManager;

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

struct AttackResult {
    int32_t monster_id;
    int32_t damage;
    bool is_kill;
};

class CombatHandler {
public:
    CombatHandler(ServerMonsterManager* monster_mgr, DropItemManager* drop_mgr);

    // 处理攻击请求
    void handle_attack_req(uint64_t player_id,
                           const uint8_t* payload, size_t payload_len,
                           Player* player, SendGameMsgFunc send_msg);

    // 怪物攻击玩家
    void monster_attack_player(uint32_t monster_id, Player* player,
                                SendGameMsgFunc send_msg);

private:
    ServerMonsterManager* monster_mgr_;
    DropItemManager* drop_mgr_;
};

}  // namespace farm
```

- [ ] **Step 4: 创建 combat_handler.cpp**

```cpp
#include "combat_handler.h"
#include "monster_manager.h"
#include "player.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <random>

namespace farm {

CombatHandler::CombatHandler(ServerMonsterManager* monster_mgr, DropItemManager* drop_mgr)
    : monster_mgr_(monster_mgr), drop_mgr_(drop_mgr) {}

void CombatHandler::handle_attack_req(uint64_t player_id,
                                       const uint8_t* payload, size_t payload_len,
                                       Player* player, SendGameMsgFunc send_msg) {
    // 解析 AttackReq
    // 简化：使用 JSON 格式
    try {
        std::string json_str(reinterpret_cast<const char*>(payload), payload_len);
        nlohmann::json req = nlohmann::json::parse(json_str);

        std::string weapon_id = req.value("weapon_id", "");
        float dir_x = req.value("dir_x", 0.0f);
        float dir_y = req.value("dir_y", 1.0f);

        // 获取玩家位置
        float px = player->get_pos_x();
        float py = player->get_pos_y();
        int32_t player_attack = player->get_attack_power();

        // 武器攻击力（简化：从配置读取）
        int32_t weapon_attack = 10;  // 默认木剑
        float attack_range = 1.5f * 16.0f;  // 1.5 tiles
        float knockback = 2.0f;

        // 攻击范围检测
        float attack_cx = px + dir_x * attack_range * 0.6f;
        float attack_cy = py + dir_y * attack_range * 0.6f;

        auto monsters = monster_mgr_->get_monsters_in_range(attack_cx, attack_cy, attack_range);

        nlohmann::json resp;
        resp["hits"] = nlohmann::json::array();

        static std::mt19937 rng(std::random_device{}());

        for (auto* m : monsters) {
            if (m->hp <= 0) continue;

            // 计算伤害
            int32_t base_damage = weapon_attack + player_attack - m->defense;
            std::uniform_real_distribution<float> dist(0.9f, 1.1f);
            int32_t damage = std::max(1, static_cast<int>(base_damage * dist(rng)));

            // 应用伤害
            int32_t remaining_hp = monster_mgr_->damage_monster(m->monster_id, damage);
            bool is_kill = remaining_hp <= 0;

            nlohmann::json hit;
            hit["monster_id"] = m->monster_id;
            hit["damage"] = damage;
            hit["remaining_hp"] = remaining_hp;
            hit["is_kill"] = is_kill;
            resp["hits"].push_back(hit);

            SPDLOG_INFO("[Combat]Player {} hit monster {} for {} damage, hp={}, kill={}",
                        player_id, m->monster_id, damage, remaining_hp, is_kill);
        }

        // 发送 AttackNotify
        std::string resp_str = resp.dump();
        send_msg(player_id, MSG_ID_ATTACK_NOTIFY,
                 reinterpret_cast<const uint8_t*>(resp_str.data()), resp_str.size());

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Combat]Failed to parse AttackReq: {}", e.what());
    }
}

void CombatHandler::monster_attack_player(uint32_t monster_id, Player* player,
                                            SendGameMsgFunc send_msg) {
    const auto* m = monster_mgr_->get_monster(monster_id);
    if (!m || m->hp <= 0) return;

    int32_t damage = std::max(1, m->attack - player->get_defense_power());
    int32_t new_hp = player->get_current_hp() - damage;
    if (new_hp < 0) new_hp = 0;

    player->set_current_hp(new_hp);

    // 发送 HP 更新
    nlohmann::json hp_update;
    hp_update["current_hp"] = new_hp;
    hp_update["max_hp"] = player->get_max_hp();
    hp_update["damage"] = damage;
    hp_update["source"] = "monster";
    hp_update["monster_id"] = monster_id;

    std::string data = hp_update.dump();
    send_msg(player->player_id(), MSG_ID_PLAYER_HP_UPDATE,
             reinterpret_cast<const uint8_t*>(data.data()), data.size());

    SPDLOG_INFO("[Combat]Monster {} hit player {} for {} damage, hp={}",
                monster_id, player->player_id(), damage, new_hp);

    if (new_hp <= 0) {
        // 玩家死亡
        nlohmann::json death_notify;
        death_notify["player_id"] = player->player_id();
        std::string death_data = death_notify.dump();
        send_msg(player->player_id(), MSG_ID_PLAYER_DEATH_NOTIFY,
                 reinterpret_cast<const uint8_t*>(death_data.data()), death_data.size());
    }
}

}  // namespace farm
```

- [ ] **Step 5: 更新 CMakeLists.txt**

在 `scripts/server/game_server/CMakeLists.txt` 的源文件列表中添加：

```cmake
    src/monster_manager.cpp
    src/combat_handler.cpp
```

- [ ] **Step 6: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 7: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/monster_manager.h scripts/server/game_server/src/monster_manager.cpp scripts/server/game_server/src/combat_handler.h scripts/server/game_server/src/combat_handler.cpp scripts/server/game_server/CMakeLists.txt
git commit -m "feat(combat): add server-side MonsterManager and CombatHandler

Add server monster manager with spawn, damage, and AI update.
Add combat handler for processing attack requests and monster
attacks on players."
```

---

### Task 8: 集成到 GameScene 和 GameRenderer（客户端）

**Files:**
- Modify: `scripts/client/game_scene.py`
- Modify: `scripts/client/game_renderer.py`
- Modify: `scripts/client/network_dispatcher.py`
- Modify: `scripts/client/interaction.py`

- [ ] **Step 1: 在 network_dispatcher.py 中添加战斗消息处理器**

在 `NetworkMessageDispatcher.__init__` 的 `_dispatch_table` 中添加：

```python
            MSG_ID_ATTACK_NOTIFY: self._handle_attack_notify,
            MSG_ID_MONSTER_SPAWN_NOTIFY: self._handle_monster_spawn,
            MSG_ID_MONSTER_DEATH_NOTIFY: self._handle_monster_death,
            MSG_ID_MONSTER_MOVE_NOTIFY: self._handle_monster_move,
            MSG_ID_MONSTER_ATTACK_NOTIFY: self._handle_monster_attack,
            MSG_ID_PLAYER_HP_UPDATE: self._handle_player_hp_update,
            MSG_ID_PLAYER_DEATH_NOTIFY: self._handle_player_death,
```

在 `__init__` 的 `_callbacks` 参数中添加对应的回调参数：

```python
        on_attack_notify=None,
        on_monster_spawn=None,
        on_monster_death=None,
        on_monster_move=None,
        on_monster_attack=None,
        on_player_hp_update=None,
        on_player_death=None,
```

在文件末尾添加对应的处理方法：

```python
    def _handle_attack_notify(self, payload: bytes):
        """处理攻击结果通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            # 使用 JSON 格式解析（简化方案）
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_attack_notify"):
                self._callbacks["on_attack_notify"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse AttackNotify: {e}")

    def _handle_monster_spawn(self, payload: bytes):
        """处理怪物刷新通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_spawn"):
                self._callbacks["on_monster_spawn"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterSpawnNotify: {e}")

    def _handle_monster_death(self, payload: bytes):
        """处理怪物死亡通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_death"):
                self._callbacks["on_monster_death"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterDeathNotify: {e}")

    def _handle_monster_move(self, payload: bytes):
        """处理怪物移动通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_move"):
                self._callbacks["on_monster_move"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterMoveNotify: {e}")

    def _handle_monster_attack(self, payload: bytes):
        """处理怪物攻击通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_attack"):
                self._callbacks["on_monster_attack"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterAttackNotify: {e}")

    def _handle_player_hp_update(self, payload: bytes):
        """处理玩家HP更新"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_player_hp_update"):
                self._callbacks["on_player_hp_update"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse PlayerHpUpdate: {e}")

    def _handle_player_death(self, payload: bytes):
        """处理玩家死亡通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_player_death"):
                self._callbacks["on_player_death"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse PlayerDeathNotify: {e}")
```

- [ ] **Step 2: 在 game_scene.py 中集成战斗系统**

在 `game_scene.py` 的导入部分添加：

```python
from .monster_manager import MonsterManager
from .weapon_manager import WeaponManager
from .combat_system import CombatSystem
from .battle_ui import BattleUI
```

在 `GameScene.__init__` 中初始化战斗系统（在 SceneManager 初始化之后）：

```python
        # 战斗系统
        self._weapon_manager = WeaponManager()
        self._monster_manager = MonsterManager()
        self._combat_system = CombatSystem(self._weapon_manager, self._monster_manager)
        self._battle_ui = BattleUI(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)

        # 玩家HP状态
        self._player_hp = player_data.get('current_hp', 100)
        self._player_max_hp = player_data.get('max_hp', 100)
        self._player_invincible_until = 0.0
```

在主循环的更新部分添加战斗系统更新：

```python
        # 更新怪物
        self._monster_manager.update(dt, self._player_sprite.world_x, self._player_sprite.world_y)

        # 更新战斗UI
        self._battle_ui.update(dt)

        # 攻击检测
        if self._input_manager.is_attack_pressed() and self._combat_system.can_attack():
            player_screen_x = self.WINDOW_WIDTH // 2
            player_screen_y = self.WINDOW_HEIGHT // 2
            attack_dir = self._input_manager.get_attack_direction(player_screen_x, player_screen_y)
            hits = self._combat_system.try_attack(
                self._player_sprite.world_x,
                self._player_sprite.world_y,
                attack_dir
            )
            for monster_id, damage, is_kill in hits:
                self._battle_ui.add_damage_number(
                    self._monster_manager.get_monster(monster_id).world_x if self._monster_manager.get_monster(monster_id) else 0,
                    self._monster_manager.get_monster(monster_id).world_y if self._monster_manager.get_monster(monster_id) else 0,
                    damage
                )
                # 发送攻击请求给服务器
                # ... (网络发送逻辑)
```

- [ ] **Step 3: 在 game_renderer.py 中添加怪物渲染和战斗UI渲染**

在 `GameRenderer.render` 方法中，在掉落物渲染之后添加怪物渲染：

```python
        # 怪物渲染
        if monster_manager is not None:
            camera_x = map_renderer.x
            camera_y = map_renderer.y
            for monster in monster_manager.get_all_monsters().values():
                monster.render(self._screen, camera_x, camera_y)
```

在对话框渲染之后添加战斗UI渲染：

```python
        # 战斗UI渲染
        if battle_ui is not None:
            camera_x = map_renderer.x
            camera_y = map_renderer.y
            battle_ui.render(self._screen, camera_x, camera_y,
                           player_hp, player_max_hp)
```

- [ ] **Step 4: 在 interaction.py 中添加矿洞入口匹配**

在 `ITEM_EFFECTS` 字典中添加：

```python
    "obj:CAVE_ENTRANCE": {
        "tool": None,
        "effect": "enter_cave",
        "description": "进入矿洞",
        "interactRange": 1,
    },
    "obj:STAIRS_DOWN": {
        "tool": None,
        "effect": "stairs_down",
        "description": "下楼",
        "interactRange": 1,
    },
    "obj:STAIRS_UP": {
        "tool": None,
        "effect": "stairs_up",
        "description": "上楼",
        "interactRange": 1,
    },
```

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/game_scene.py scripts/client/game_renderer.py scripts/client/network_dispatcher.py scripts/client/interaction.py
git commit -m "feat(combat): integrate combat system into GameScene

Wire up MonsterManager, CombatSystem, WeaponManager, BattleUI into
GameScene. Add combat message handlers to NetworkMessageDispatcher.
Add cave/stairs interaction rules."
```

---

### Task 9: Protobuf消息定义与数据同步

**Files:**
- Modify: `shared/proto/player.proto` (如果存在)
- Modify: `scripts/server/game_server/src/game_server.cpp`

- [ ] **Step 1: 在 player.proto 中添加战斗消息定义**

在 `shared/proto/player.proto` 文件末尾添加：

```protobuf
// 攻击请求
message AttackReq {
    float dir_x = 1;
    float dir_y = 2;
    string weapon_id = 3;
}

// 攻击结果通知
message AttackNotify {
    repeated AttackHit hits = 1;
}

message AttackHit {
    uint32 monster_id = 1;
    int32 damage = 2;
    int32 remaining_hp = 3;
    bool is_kill = 4;
}

// 怪物刷新通知
message MonsterSpawnNotify {
    uint32 monster_id = 1;
    string monster_type = 2;
    float x = 3;
    float y = 4;
    int32 hp = 5;
    int32 max_hp = 6;
}

// 怪物死亡通知
message MonsterDeathNotify {
    uint32 monster_id = 1;
    repeated DropEntry drops = 2;
    int32 exp_reward = 3;
}

message DropEntry {
    int32 item_id = 1;
    int32 count = 2;
}

// 怪物移动通知
message MonsterMoveNotify {
    uint32 monster_id = 1;
    float x = 2;
    float y = 3;
    string state = 4;
}

// 玩家HP更新
message PlayerHpUpdate {
    int32 current_hp = 1;
    int32 max_hp = 2;
    int32 damage = 3;
    string source = 4;
}

// 玩家死亡通知
message PlayerDeathNotify {
    uint64 player_id = 1;
    repeated LostItem lost_items = 2;
}

message LostItem {
    int32 item_id = 1;
    int32 count = 2;
}
```

- [ ] **Step 2: 重新编译 protobuf**

```bash
cd D:/mb_workspace/farm_demo
python -m grpc_tools.protoc -Ishared/proto --python_out=scripts/common/proto/generated --cpp_out=scripts/server/common/include shared/proto/player.proto
```

- [ ] **Step 3: 在 game_server.cpp 中注册战斗消息handler**

在 `game_server.cpp` 的 `start()` 方法中，在位置更新 handler 注册之后添加：

```cpp
    // 注册攻击请求消息处理
    msg_handler_.register_handler(MSG_ID_ATTACK_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
            if (!player) return;
            auto send_msg = [this](uint64_t pid, uint32_t msg_id, const uint8_t* p, size_t l) {
                send_game_msg(pid, msg_id, p, l);
            };
            combat_handler_->handle_attack_req(player_id, payload, payload_len, player, send_msg);
        });
    SPDLOG_INFO("[Game]Attack handler registered");
```

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add shared/proto/player.proto scripts/common/proto/generated/ scripts/server/game_server/src/game_server.cpp
git commit -m "feat(combat): add protobuf messages and register attack handler

Add protobuf definitions for attack, monster sync, player HP, and
death messages. Register attack handler in GameServer."
```

---

### Task 10: 测试与验证

**Files:**
- Create: `scripts/client/test_combat_system.py`

- [ ] **Step 1: 编写战斗系统单元测试**

```python
"""
战斗系统单元测试
"""
import unittest
import time
from unittest.mock import MagicMock, patch

from scripts.client.weapon_manager import WeaponManager
from scripts.client.monster_manager import MonsterManager
from scripts.client.combat_system import CombatSystem
from scripts.client.monster_sprite import MonsterSprite, MonsterState


class TestWeaponManager(unittest.TestCase):
    def test_load_weapons(self):
        wm = WeaponManager()
        # 如果数据文件存在，应该加载成功
        weapons = wm.get_all_weapons()
        # 至少有默认武器
        self.assertIsInstance(weapons, list)

    def test_is_weapon(self):
        wm = WeaponManager()
        # item_id 100 应该是木剑
        if wm.get_weapon_by_item(100):
            self.assertTrue(wm.is_weapon(100))
        self.assertFalse(wm.is_weapon(999))


class TestMonsterManager(unittest.TestCase):
    def test_spawn_monster(self):
        mm = MonsterManager()
        # 如果数据文件存在
        if mm.get_monster_def("slime"):
            m = mm.spawn_monster(1, "slime", 100, 100, 30, 30)
            self.assertIsNotNone(m)
            self.assertEqual(m.monster_id, 1)
            self.assertEqual(m.hp, 30)

    def test_remove_monster(self):
        mm = MonsterManager()
        if mm.get_monster_def("slime"):
            mm.spawn_monster(1, "slime", 100, 100, 30, 30)
            mm.remove_monster(1)
            self.assertIsNone(mm.get_monster(1))


class TestCombatSystem(unittest.TestCase):
    def test_can_attack_without_weapon(self):
        wm = WeaponManager()
        mm = MonsterManager()
        cs = CombatSystem(wm, mm)
        self.assertFalse(cs.can_attack())

    def test_equip_weapon(self):
        wm = WeaponManager()
        mm = MonsterManager()
        cs = CombatSystem(wm, mm)
        cs.equip_weapon("wooden_sword")
        self.assertIsNotNone(cs.equipped_weapon)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 运行测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_combat_system.py -v
```

- [ ] **Step 3: 编译服务器**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/test_combat_system.py
git commit -m "test(combat): add combat system unit tests

Add tests for WeaponManager, MonsterManager, and CombatSystem."
```

---

## Phase 2: 矿洞场景（后续）

Phase 2 包含矿洞层配置、矿洞入口、场景切换、光照效果、怪物刷新机制。这些任务在 Phase 1 完成后再实施。

## Phase 3: 进阶系统（后续）

Phase 3 包含 Boss 怪物、武器合成系统、武器强化系统、战斗经验+升级。这些任务在 Phase 2 完成后再实施。
