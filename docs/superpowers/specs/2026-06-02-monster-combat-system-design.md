# 怪兽与战斗系统设计

**日期**: 2026-06-02
**状态**: 已批准
**范围**: 完整功能 — 多层矿洞、多种怪物、Boss 战、武器升级

---

## 1. 概述

为农场游戏添加怪兽和战斗系统，参考星露谷物语的矿洞战斗玩法。玩家进入专用矿洞场景，与怪物战斗获取掉落物和经验值，逐层深入挑战更强敌人和 Boss。

### 1.1 功能清单

| 功能 | 描述 |
|------|------|
| 怪物系统 | 多种怪物（史莱姆、蝙蝠、骷髅），数据驱动定义 |
| 怪物 AI | 简单游荡 + 靠近追击状态机 |
| 战斗系统 | 挥砍攻击，武器属性，伤害计算，击退效果 |
| 玩家 HP | 生命值系统，受伤、死亡、复活机制 |
| 死亡惩罚 | 传回农场，丢失 30% 背包物品 |
| 掉落系统 | 怪物死亡掉落物品，概率配置 |
| 经验升级 | 战斗经验值，升级增加最大 HP |
| 矿洞场景 | 多层矿洞，逐层深入，环境光照 |
| Boss 怪物 | 阶段战斗，特殊攻击，召唤小怪 |
| 武器系统 | 武器合成、强化，材料收集 |

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      GameScene                          │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐    ┌───────────┐ │
│  │ Monster      │───▶│ Combat       │───▶│ Battle    │ │
│  │ Manager      │    │ System       │    │ UI        │ │
│  └──────────────┘    └──────────────┘    └───────────┘ │
│       │                    │                   │        │
│       ▼                    ▼                   ▼        │
│  ┌──────────────┐    ┌──────────────┐    ┌───────────┐ │
│  │ Monster      │    │ Weapon       │    │ HP Bar    │ │
│  │ Sprite       │    │ Manager      │    │ UI        │ │
│  └──────────────┘    └──────────────┘    └───────────┘ │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐                   │
│  │ Cave         │    │ Drop         │                   │
│  │ Scene Mgr    │    │ Manager      │                   │
│  └──────────────┘    └──────────────┘                   │
└─────────────────────────────────────────────────────────┘
```

### 2.1 模块职责

| 模块 | 职责 |
|------|------|
| **MonsterManager** | 管理怪物实例，根据配置生成怪物，驱动怪物 AI（游荡+追击） |
| **MonsterSprite** | 怪物的视觉表现（精灵动画、HP 条、受击闪白） |
| **CombatSystem** | 处理挥砍攻击、伤害计算、击杀判定 |
| **WeaponManager** | 管理武器数据（攻击力、攻击范围、攻击速度） |
| **CaveSceneManager** | 矿洞场景管理（多层、刷新规则、波次机制） |
| **BattleUI** | 战斗相关 UI（玩家 HP 条、怪物 HP 条、伤害数字） |

### 2.2 数据驱动

怪物定义、武器定义、掉落表、矿洞层配置全部用 JSON 配置文件定义，代码为通用引擎。与现有 NPC 对话系统架构一致。

---

## 3. 怪物系统

### 3.1 怪物定义（monster_defs.json）

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
      "wander_radius": 3
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
      "wander_radius": 5
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
      "wander_radius": 2
    }
  ]
}
```

### 3.2 怪物属性说明

| 属性 | 说明 |
|------|------|
| id | 唯一标识符 |
| name | 显示名称 |
| sprite_sheet | 精灵图文件名 |
| hp | 生命值 |
| attack | 攻击力 |
| defense | 防御力 |
| speed | 移动速度（像素/秒） |
| exp_reward | 击杀经验奖励 |
| aggro_range | 仇恨范围（格） |
| attack_range | 攻击范围（格） |
| attack_cooldown | 攻击冷却（秒） |
| wander_interval | 游荡间隔范围 [min, max]（秒） |
| wander_radius | 游荡半径（格） |

### 3.3 怪物 AI 状态机

```
┌─────────┐    玩家进入范围    ┌─────────┐
│  IDLE   │──────────────────▶│  CHASE  │
│ (游荡)  │◀──────────────────│ (追击)  │
└─────────┘    玩家离开范围    └─────────┘
     │                             │
     │ 定时随机移动                 │ 到达攻击范围
     ▼                             ▼
┌─────────┐                  ┌─────────┐
│ WANDER  │                  │ ATTACK  │
│ (闲逛)  │                  │ (攻击)  │
└─────────┘                  └─────────┘
```

**状态说明**：

| 状态 | 行为 |
|------|------|
| IDLE | 原地等待，定时切换到 WANDER |
| WANDER | 在刷新点附近随机移动，检测玩家距离 |
| CHASE | 直线追击玩家，每帧更新方向 |
| ATTACK | 对玩家造成伤害，进入冷却 |

### 3.4 怪物刷新机制

- 每个矿洞层配置最大怪物数量和刷新点
- 怪物死亡后，延迟 N 秒在随机刷新点重新生成
- 玩家离开场景时冻结怪物状态（复用现有 freeze/thaw 机制）
- Boss 房间不自动刷新 Boss

---

## 4. 战斗系统

### 4.1 武器定义（weapon_defs.json）

```json
{
  "weapons": [
    {
      "id": "wooden_sword",
      "name": "木剑",
      "icon": "item_sword_wood.png",
      "attack": 10,
      "attack_range": 1.5,
      "attack_speed": 0.4,
      "knockback": 2.0,
      "description": "基础的木质短剑"
    },
    {
      "id": "iron_sword",
      "name": "铁剑",
      "icon": "item_sword_iron.png",
      "attack": 25,
      "attack_range": 1.8,
      "attack_speed": 0.35,
      "knockback": 3.0,
      "description": "坚固的铁制长剑"
    },
    {
      "id": "gold_sword",
      "name": "金剑",
      "icon": "item_sword_gold.png",
      "attack": 40,
      "attack_range": 2.0,
      "attack_speed": 0.3,
      "knockback": 4.0,
      "description": "闪耀的黄金之剑"
    }
  ]
}
```

### 4.2 武器属性说明

| 属性 | 说明 |
|------|------|
| id | 唯一标识符 |
| name | 显示名称 |
| icon | 图标文件名 |
| attack | 基础攻击力 |
| attack_range | 攻击范围（格） |
| attack_speed | 攻击动画时长（秒） |
| knockback | 击退距离（格） |

### 4.3 攻击流程

**攻击触发**：
- 鼠标左键：向鼠标方向挥砍
- E 键：向玩家当前朝向挥砍
- 需要装备武器才能攻击

```
玩家按攻击键（鼠标左键 或 E键）
       │
       ▼
检查武器装备 & 攻击冷却
       │
       ▼ 计算攻击范围
生成攻击判定区域（面向方向的扇形/矩形）
       │
       ▼ 检测命中
遍历范围内怪物，计算伤害
       │
       ▼ 伤害公式
伤害 = 武器攻击力 + 玩家装备攻击力 - 怪物防御力 + 随机浮动(±10%)
       │
       ▼ 应用效果
扣减怪物HP + 击退效果 + 受击闪白
       │
       ▼ 检查击杀
怪物HP ≤ 0 → 触发死亡流程
```

### 4.4 伤害公式

```
base_damage = weapon_attack + player_equip_attack - monster_defense
random_factor = random(0.9, 1.1)
final_damage = max(1, floor(base_damage * random_factor))
```

### 4.5 击退效果

- 怪物被击中后沿攻击方向后退 N 格
- 击退期间怪物无法行动
- 击退距离由武器的 `knockback` 属性决定

### 4.6 服务器验证

- 客户端发送攻击请求（攻击方向、武器ID）
- 服务器验证：玩家位置、武器合法性、冷却时间
- 服务器计算伤害并广播结果给附近玩家

---

## 5. 玩家 HP 与死亡

### 5.1 玩家 HP 系统

**扩展 PlayerBizData**（服务器端）：

```cpp
struct PlayerBizData {
    // ... 现有字段 ...
    int32_t max_hp = 100;        // 最大生命值
    int32_t current_hp = 100;    // 当前生命值
    int32_t attack_power = 0;    // 额外攻击力（装备加成）
    int32_t defense_power = 0;   // 额外防御力（装备加成）
    int32_t combat_exp = 0;      // 战斗经验值
    int32_t combat_level = 1;    // 战斗等级
};
```

### 5.2 受伤流程

```
怪物攻击玩家
       │
       ▼
伤害 = 怪物攻击力 - 玩家防御力 + 随机浮动
       │
       ▼
扣减 current_hp，触发受伤无敌帧（0.5秒）
       │
       ▼
播放受伤动画 + 屏幕闪红
       │
       ▼
检查 current_hp ≤ 0
       │
       ▼ 是
触发死亡流程
```

### 5.3 死亡惩罚

```
玩家 HP 归零
       │
       ▼
播放死亡动画
       │
       ▼
弹出提示："你被击败了，丢失了部分物品..."
       │
       ▼
随机丢失背包中 30% 物品数量（按物品格子计算，向下取整，每种物品至少保留1个）
       │
       ▼
传送回农场出生点
       │
       ▼
恢复 HP 至 50%
```

### 5.4 HP 恢复方式

| 方式 | 恢复量 | 条件 |
|------|--------|------|
| 睡觉 | 恢复至满 | 每天结束时自动触发 |
| 食物 | +20~50 | 使用食物类物品 |
| 药水 | +100 | 使用药水类物品 |

---

## 6. 奖励系统

### 6.1 掉落表（drop_table.json）

```json
{
  "drop_tables": {
    "slime": [
      {"item_id": "slime_gel", "count": [1, 3], "chance": 0.8},
      {"item_id": "coin", "count": [5, 15], "chance": 1.0},
      {"item_id": "hp_potion_small", "count": [1, 1], "chance": 0.15}
    ],
    "bat": [
      {"item_id": "bat_wing", "count": [1, 2], "chance": 0.7},
      {"item_id": "coin", "count": [8, 20], "chance": 1.0},
      {"item_id": "hp_potion_small", "count": [1, 1], "chance": 0.2}
    ],
    "skeleton": [
      {"item_id": "bone", "count": [1, 3], "chance": 0.9},
      {"item_id": "iron_ore", "count": [1, 2], "chance": 0.4},
      {"item_id": "coin", "count": [20, 50], "chance": 1.0},
      {"item_id": "hp_potion_large", "count": [1, 1], "chance": 0.1},
      {"item_id": "iron_sword", "count": [1, 1], "chance": 0.05}
    ],
    "boss_skeleton_king": [
      {"item_id": "crown_fragment", "count": [1, 1], "chance": 1.0},
      {"item_id": "gold_ore", "count": [3, 5], "chance": 0.8},
      {"item_id": "coin", "count": [100, 200], "chance": 1.0},
      {"item_id": "gold_sword", "count": [1, 1], "chance": 0.15}
    ]
  }
}
```

### 6.2 掉落物拾取

- 怪物死亡后在原地生成掉落物精灵
- 掉落物有轻微随机散开效果
- 玩家靠近自动拾取（复用现有 DropItemManager）
- 金币直接进入背包/计数器

### 6.3 经验值与升级

**战斗等级表**：

| 等级 | 所需经验 | 累计经验 | 奖励 |
|------|----------|----------|------|
| 1→2 | 50 | 50 | +5 最大HP |
| 2→3 | 120 | 170 | +5 最大HP |
| 3→4 | 200 | 370 | +10 最大HP |
| 4→5 | 350 | 720 | +10 最大HP, 解锁铁剑配方 |
| 5→6 | 500 | 1220 | +15 最大HP |
| ... | 递增 | ... | 每级增加HP，特定等级解锁配方 |

**升级公式**：`需要经验 = 50 + (level - 1) * 30 + level^2 * 10`

### 6.4 升级效果

- 每次升级增加最大 HP
- 特定等级解锁武器合成配方
- 升级时播放特效和音效

---

## 7. 矿洞场景系统

### 7.1 矿洞层配置（cave_levels.json）

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
      "light_radius": 5
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
      "light_radius": 4
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
      "light_radius": 3
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
      "light_radius": 6
    }
  ]
}
```

### 7.2 矿洞入口

- 农场地图上放置一个矿洞入口（ObjectType.CAVE_ENTRANCE）
- 交互后进入矿洞第 1 层
- 矿洞内有楼梯（exit_down/exit_up）连接各层

### 7.3 矿洞光照效果

- 矿洞场景使用暗色环境光
- 玩家周围有圆形光照范围
- 光照半径随层数加深而减小
- 使用 Pygame surface 混合实现

### 7.4 场景切换流程

```
玩家站在楼梯上按空格
       │
       ▼
客户端发送场景切换请求（目标层ID）
       │
       ▼
服务器验证：玩家在楼梯位置
       │
       ▼
服务器保存当前层状态，加载目标层
       │
       ▼
客户端收到响应，切换场景渲染
       │
       ▼
玩家出现在目标层的入口位置
```

---

## 8. Boss 怪物系统

### 8.1 Boss 定义（boss_defs.json）

```json
{
  "bosses": [
    {
      "id": "boss_skeleton_king",
      "name": "骷髅王",
      "sprite_sheet": "boss_skeleton_king.png",
      "hp": 500,
      "attack": 25,
      "defense": 10,
      "speed": 20.0,
      "exp_reward": 500,
      "size": [2, 2],
      "phases": [
        {
          "hp_threshold": 1.0,
          "attack_pattern": "normal",
          "speed_multiplier": 1.0,
          "description": "普通阶段"
        },
        {
          "hp_threshold": 0.5,
          "attack_pattern": "aggressive",
          "speed_multiplier": 1.3,
          "summon_monsters": ["skeleton", "skeleton"],
          "description": "狂暴阶段：召唤小怪，速度提升"
        }
      ],
      "special_attacks": [
        {
          "id": "ground_slam",
          "name": "地震",
          "cooldown": 8.0,
          "range": 3,
          "damage_multiplier": 2.0,
          "aoe_radius": 2,
          "animation": "boss_slam"
        }
      ]
    }
  ]
}
```

### 8.2 Boss 战斗机制

**阶段转换**：
- Boss HP 降至 50% 时进入第二阶段
- 阶段转换时播放特效，短暂无敌
- 第二阶段：速度提升、召唤小怪

**特殊攻击**：
- 地震（ground_slam）：范围 AOE，玩家需要闪避
- 特殊攻击有明显前摇动画，给玩家反应时间
- 冷却时间防止连续释放

**Boss 房间**：
- 独立场景，空间较大
- 进入时关闭入口门，击败 Boss 后开门
- Boss 房间有独特的背景音乐（后续扩展）

### 8.3 Boss 击败奖励

- 固定掉落稀有物品（如王冠碎片）
- 高额经验值
- 概率掉落高级武器
- 首次击杀额外奖励（成就系统扩展）

---

## 9. 武器升级系统

### 9.1 武器配方（weapon_recipes.json）

```json
{
  "weapon_recipes": [
    {
      "result": "iron_sword",
      "materials": [
        {"item_id": "iron_ore", "count": 5},
        {"item_id": "wood", "count": 3}
      ],
      "unlock_level": 5,
      "craft_station": "anvil"
    },
    {
      "result": "gold_sword",
      "materials": [
        {"item_id": "gold_ore", "count": 5},
        {"item_id": "iron_ore", "count": 3},
        {"item_id": "diamond", "count": 1}
      ],
      "unlock_level": 10,
      "craft_station": "anvil"
    }
  ]
}
```

### 9.2 武器强化

**强化材料**（从矿洞怪物掉落）：

| 材料 | 来源 | 用途 |
|------|------|------|
| 精炼石 | 矿洞 2-3 层怪物 | 强化武器 +1~+3 |
| 高级精炼石 | Boss 掉落 | 强化武器 +4~+6 |
| 传说精炼石 | Boss 首杀/稀有掉落 | 强化武器 +7~+10 |

**强化规则**：

| 当前等级 | 成功率 | 失败惩罚 |
|----------|--------|----------|
| +0 → +1 | 100% | 无 |
| +1 → +2 | 90% | 无 |
| +2 → +3 | 80% | 无 |
| +3 → +4 | 70% | 等级不变 |
| +4 → +5 | 60% | 等级不变 |
| +5 → +6 | 50% | 等级 -1 |
| +6 → +7 | 40% | 等级 -1 |
| +7 → +8 | 30% | 等级 -2 |
| +8 → +9 | 20% | 等级 -2 |
| +9 → +10 | 10% | 等级 -3 |

**强化加成**：
- 每级强化：攻击力 +5%，攻击速度 +2%
- +5 额外效果：武器发光
- +10 额外效果：武器特效（粒子效果）

### 9.3 合成界面

- 铁砧交互打开合成界面
- 显示可合成武器列表（灰色锁定/彩色可合成）
- 材料不足时显示缺少的材料
- 合成成功播放特效

---

## 10. 消息协议

### 10.1 新增消息 ID

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
MSG_ID_COMBAT_LEVEL_UP = 5031,      // 战斗升级
MSG_ID_WEAPON_EQUIP_REQ = 5040,     // 装备武器请求
MSG_ID_WEAPON_EQUIP_RESP = 5041,    // 装备武器响应
MSG_ID_WEAPON_CRAFT_REQ = 5050,     // 武器合成请求
MSG_ID_WEAPON_CRAFT_RESP = 5051     // 武器合成响应
```

### 10.2 核心数据流

**攻击流程**：
```
Client                          Server
  │                               │
  │──── AttackReq ───────────────▶│ 验证攻击合法性
  │    {direction, weapon_id}     │ 计算伤害
  │                               │ 更新怪物HP
  │◀── AttackNotify ─────────────│
  │    {monster_id, damage, hp}   │
  │                               │
  │                               │ 如果怪物死亡
  │◀── MonsterDeathNotify ───────│
  │    {monster_id, drops, exp}   │
```

**怪物同步**：
```
Server (每 200ms)                 Client
  │                               │
  │── MonsterMoveNotify ─────────▶│ 更新怪物位置
  │   {monster_id, x, y, state}  │
  │                               │
  │── MonsterAttackNotify ───────▶│ 播放怪物攻击动画
  │   {monster_id, damage}        │ 扣减玩家HP
```

### 10.3 性能考虑

- 怪物移动使用 200ms 同步间隔（与玩家位置同步类似）
- 客户端使用插值平滑怪物移动
- 只同步视野范围内的怪物（复用现有 AOI 逻辑）
- Boss 战斗使用独立同步通道，降低延迟

---

## 11. 实现范围

### Phase 1: 基础战斗（核心）
- 怪物定义 JSON + 加载
- 怪物精灵渲染
- 怪物 AI（游荡+追击）
- 玩家挥砍攻击
- 伤害计算 + 击退
- 怪物死亡 + 掉落
- 玩家 HP + 受伤
- 玩家死亡 + 惩罚

### Phase 2: 矿洞场景
- 矿洞层配置
- 矿洞入口（农场→矿洞）
- 场景切换（层间移动）
- 矿洞光照效果
- 怪物刷新机制

### Phase 3: 进阶系统
- Boss 怪物（阶段、特殊攻击）
- 武器合成系统
- 武器强化系统
- 战斗经验 + 升级
