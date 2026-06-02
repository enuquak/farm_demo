# Quest System Design

**Date:** 2026-06-02
**Status:** Draft
**Author:** AI Assistant

## 1. Overview

This document describes the design of a quest/task system for the farm_demo project. The system includes:

- **Quest System**: Server-authoritative quest management with branching support
- **Event System**: General-purpose event bus for decoupling game systems
- **Quest Editor**: Python desktop GUI for quest configuration

## 2. Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Quest Editor (Python/tkinter)      │
│  可视化节点图编辑 → 输出 quest_data.json             │
└──────────────────────┬──────────────────────────────┘
                       │ 读取/写入
                       ▼
┌─────────────────────────────────────────────────────┐
│              Quest Data (JSON 静态配置)              │
│  quest_data.json: 任务定义、条件、奖励、分支关系     │
└──────────────────────┬──────────────────────────────┘
                       │ 服务端加载
                       ▼
┌─────────────────────────────────────────────────────┐
│              Game Server (C++ 运行时)                │
│  ┌───────────┐  ┌───────────┐  ┌─────────────────┐ │
│  │ Quest     │  │ Event     │  │ Quest Progress  │ │
│  │ Manager   │←→│ Bus       │←→│ (PlayerData)    │ │
│  └───────────┘  └───────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────┘
```

**Data Flow:**
1. Editor produces `quest_data.json` (static quest configuration)
2. Server loads quest configuration at startup
3. QuestManager manages quest state based on configuration and player progress
4. Game events (collection, planting, dialogue, etc.) notify QuestManager via EventBus
5. On quest completion, QuestManager triggers reward distribution via EventBus
6. Player quest progress is persisted with PlayerData

**Note:** This is a large feature with 5 implementation phases. Each phase should be completed and tested before moving to the next.

## 3. Quest Data Model

### 3.1 Quest State Machine

```
NOT_ACCEPTED → ACCEPTED → IN_PROGRESS → COMPLETED
                    │            │
                    ▼            ▼
                  FAILED ← ─ ─ ─┘
```

- **NOT_ACCEPTED**: Quest is unlocked but not accepted
- **ACCEPTED**: Accepted but not started (e.g., dialogue quest, accepted then go do it)
- **IN_PROGRESS**: In progress (progress > 0)
- **COMPLETED**: Completed
- **FAILED**: Failed (optional, supports quest failure scenarios)

### 3.2 Objective Types

| Type | Description | Example |
|---|---|---|
| `collect_item` | Collect specified items | Collect 10 wood |
| `plant_crop` | Plant specified crops | Plant 3 wheat |
| `harvest_crop` | Harvest specified crops | Harvest 3 wheat |
| `talk_to_npc` | Talk to specified NPC | Talk to farmer |
| `visit_scene` | Visit specified scene | Visit mine |
| `craft_item` | Craft specified items | Craft 2 iron ingots |
| `use_item` | Use specified items | Use 5 fertilizers |
| `reach_level` | Reach specified level | Reach level 5 |
| `own_gold` | Own specified gold | Own 1000 gold |
| `custom` | Custom condition | Evaluate custom expression (e.g., "player_level >= 10 AND gold >= 500") |

### 3.3 Event Mapping

| Objective Type | Trigger Event |
|---|---|
| `collect_item` | `ItemCollected` |
| `plant_crop` | `CropPlanted` |
| `harvest_crop` | `CropHarvested` |
| `talk_to_npc` | `NPCTalked` |
| `visit_scene` | `SceneVisited` |
| `craft_item` | `ItemCrafted` |
| `use_item` | `ItemUsed` |
| `reach_level` | `LevelUp` |
| `own_gold` | `GoldChanged` |
| `custom` | `CustomConditionMet` |

### 3.4 JSON Configuration Structure

```json
{
  "quests": {
    "quest_001": {
      "id": "quest_001",
      "name": "初识农耕",
      "description": "学习基本的种植技巧",
      "type": "plant_harvest",
      "trigger": {
        "type": "auto",
        "condition": "player_level >= 1"
      },
      "trigger_type": "auto",
      "objectives": [
        {
          "id": "obj_001",
          "type": "plant_crop",
          "target": "wheat",
          "count": 3,
          "description": "种植3个小麦"
        },
        {
          "id": "obj_002",
          "type": "harvest_crop",
          "target": "wheat",
          "count": 3,
          "description": "收获3个小麦"
        }
      ],
      "rewards": {
        "items": [{"id": "seed_tomato", "count": 5}],
        "gold": 100,
        "exp": 50,
        "unlock_quests": ["quest_002", "quest_003"]
      },
      "prerequisites": [],
      "branch_group": null
    },
    "quest_002": {
      "id": "quest_002",
      "name": "番茄之路",
      "description": "尝试种植番茄",
      "type": "plant_harvest",
      "trigger": {
        "type": "npc",
        "npc_id": "npc_farmer",
        "dialogue": "你已经学会了种小麦，试试番茄吧！"
      },
      "trigger_type": "npc",
      "objectives": [
        {
          "id": "obj_003",
          "type": "plant_crop",
          "target": "tomato",
          "count": 5,
          "description": "种植5个番茄"
        }
      ],
      "rewards": {
        "items": [{"id": "fertilizer", "count": 10}],
        "gold": 150,
        "exp": 75,
        "unlock_quests": ["quest_004"]
      },
      "prerequisites": ["quest_001"],
      "branch_group": "farming_path"
    }
  },
  "branch_groups": {
    "farming_path": {
      "name": "农耕路线",
      "description": "专注于农业发展",
      "unlock_condition": "quest_001.completed"
    }
  }
}
```

### 3.5 Player Quest Progress Data

Add `taskInfos` field to `PlayerBizData`:

```protobuf
message TaskInfo {
  string task_id = 1;
  TaskState state = 2;  // NOT_ACCEPTED, ACCEPTED, IN_PROGRESS, COMPLETED, FAILED
  map<string, int32> progress = 3;  // objective_id -> current_count
  int64 accept_time = 4;
  int64 complete_time = 5;
}

enum TaskState {
  NOT_ACCEPTED = 0;
  ACCEPTED = 1;
  IN_PROGRESS = 2;
  COMPLETED = 3;
  FAILED = 4;
}
```

## 4. Event System (EventBus)

### 4.1 Architecture

```
┌─────────────────────────────────────────────────┐
│                 EventBus (单例)                   │
│                                                   │
│  subscribe(event_type, handler)                  │
│  unsubscribe(event_type, handler)                │
│  emit(event_type, event_data)                    │
│                                                   │
│  ┌─────────────┐  ┌─────────────┐               │
│  │ Handler List │  │ Handler List │               │
│  │ "ItemUsed"   │  │ "CropPlanted"│  ...          │
│  └─────────────┘  └─────────────┘               │
└─────────────────────────────────────────────────┘
        ▲                    ▲
        │ emit               │ subscribe
        │                    │
┌───────┴───────┐    ┌───────┴───────┐
│ Game Systems  │    │ QuestManager  │
│ (产生事件)     │    │ (监听事件)     │
└───────────────┘    └───────────────┘
```

### 4.2 Event Type Definition

```cpp
// Event data structure
struct GameEvent {
    EventType type;
    uint32_t player_id;
    json data;  // Event-specific data
};

// Event type enum
enum class EventType {
    // Item related
    ItemCollected,    // {item_id, count}
    ItemUsed,         // {item_id, count}
    ItemCrafted,      // {item_id, count}
    
    // Agriculture related
    CropPlanted,      // {crop_id, x, y}
    CropHarvested,    // {crop_id, count}
    
    // Interaction related
    NPCTalked,        // {npc_id}
    SceneVisited,     // {scene_id}
    
    // Player related
    LevelUp,          // {new_level}
    GoldChanged,      // {new_amount}
    
    // Quest related
    QuestAccepted,    // {quest_id}
    QuestCompleted,   // {quest_id}
    QuestFailed,      // {quest_id}
    
    // Reward related
    RewardGranted,    // {quest_id, reward_type, reward_data}
    
    // Custom
    Custom            // {condition_id, result}
};
```

### 4.3 Usage Example

```cpp
// Register event listener
event_bus.subscribe(EventType::CropHarvested, 
    [this](const GameEvent& e) {
        on_crop_harvested(e.player_id, e.data);
    });

// Emit event
event_bus.emit(EventType::CropHarvested, player_id, {
    {"crop_id", "wheat"},
    {"count", 3}
});
```

### 4.4 Reward Distribution Flow

```
Quest Completed → EventBus::emit(QuestCompleted)
                ↓
        QuestManager receives event
                ↓
        Check reward configuration
                ↓
        ┌─────────────────────────────────────┐
        │ Reward type dispatch                 │
        ├─────────┬─────────┬─────────┬───────┤
        │ Items   │ Gold    │ Exp     │Unlock │
        │ GiveItem│ SetGold │ SetExp  │Unlock │
        └─────────┴─────────┴─────────┴───────┘
                ↓
        EventBus::emit(RewardGranted)
```

## 5. QuestManager

### 5.1 Core Responsibilities

```
┌─────────────────────────────────────────────────────────┐
│                    QuestManager                          │
│                                                           │
│  - 加载任务配置 (quest_data.json)                        │
│  - 管理玩家任务状态                                       │
│  - 监听游戏事件，更新任务进度                             │
│  - 检测任务完成，触发奖励                                 │
│  - 处理任务接取/提交请求                                  │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐ │
│  │  PlayerQuestData                                    │ │
│  │  - quest_states: map<quest_id, TaskState>           │ │
│  │  - quest_progress: map<quest_id, map<obj_id, int>>  │ │
│  │  - active_quests: list<quest_id>                    │ │
│  └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 5.2 Quest Lifecycle

```
1. Player Login
   └→ QuestManager::on_player_login(player_id)
       └→ Load taskInfos from PlayerData
       └→ Check auto-trigger quests

2. Game Event Occurs
   └→ EventBus::emit(event_type, data)
       └→ QuestManager::on_event(player_id, event_type, data)
           └→ Iterate active quests
           └→ Match objective type and event type
           └→ Update progress
           └→ Check completion

3. Quest Completed
   └→ QuestManager::on_quest_completed(player_id, quest_id)
       └→ Mark quest as COMPLETED
       └→ Grant rewards (items, gold, exp)
       └→ Unlock subsequent quests
       └→ EventBus::emit(QuestCompleted)

4. Player Logout
   └→ QuestManager::on_player_logout(player_id)
       └→ Quest progress saved with PlayerData
```

### 5.3 Message Protocol

```protobuf
// Client → Server
message QuestAcceptReq {
  string quest_id = 1;
}

message QuestSubmitReq {
  string quest_id = 1;
}

message QuestAbandonReq {
  string quest_id = 1;
}

// Server → Client
message QuestAcceptResp {
  int32 result = 1;
  TaskInfo task_info = 2;
}

message QuestSubmitResp {
  int32 result = 1;
  QuestReward rewards = 2;
}

message QuestSyncNotify {
  repeated TaskInfo task_infos = 1;
}

message QuestProgressNotify {
  string quest_id = 1;
  map<string, int32> progress = 2;
}

message QuestReward {
  repeated ItemInfo items = 1;
  int32 gold = 2;
  int32 exp = 3;
  repeated string unlock_quests = 4;
}
```

## 6. Quest Editor (Python GUI)

### 6.1 Technical Stack

- **GUI Framework**: tkinter (Python standard library, no additional dependencies)
- **Graphics Drawing**: tkinter Canvas (draw nodes and connections)
- **Data Format**: JSON file read/write

### 6.2 UI Layout

```
┌─────────────────────────────────────────────────────────────┐
│  Menu Bar: File | Edit | View | Tools                        │
├─────────────────────────────────────────────────────────────┤
│  Toolbar: [New] [Open] [Save] [Add Quest] [Connect] [Delete] │
├───────────────────────┬─────────────────────────────────────┤
│                       │                                     │
│   Quest Node Graph    │   Property Edit Panel               │
│   (Canvas)            │                                     │
│                       │   Quest Name: [初识农耕          ]  │
│   ┌─────┐            │   Quest Type: [种植/收获    ▼]      │
│   │Q001 │───┐        │   Trigger:    [自动触发      ▼]      │
│   └─────┘   │        │                                     │
│             ▼        │   ── Objectives ──                   │
│   ┌─────┐  ┌─────┐   │   Type: [种植作物    ▼]              │
│   │Q002 │  │Q003 │   │   Target: [小麦          ]           │
│   └─────┘  └─────┘   │   Count: [3              ]           │
│                       │                                     │
│                       │   ── Rewards ──                      │
│                       │   Items: [种子番茄 x5    ] [+]       │
│                       │   Gold:  [100            ]           │
│                       │   Exp:   [50             ]           │
│                       │   Unlock: [Q002, Q003     ]           │
│                       │                                     │
├───────────────────────┴─────────────────────────────────────┤
│  Status Bar: Loaded 3 quests | Unsaved changes               │
└─────────────────────────────────────────────────────────────┘
```

### 6.3 Core Features

1. **Node Graph Editing**
   - Drag to create/move quest nodes
   - Connect lines for prerequisite dependencies
   - Right-click menu: add, delete, copy nodes
   - Zoom and pan canvas

2. **Property Editing**
   - Property panel displays when node is selected
   - Edit quest name, description, type
   - Add/delete/modify objectives
   - Configure rewards
   - Set trigger conditions

3. **File Operations**
   - New quest configuration file
   - Open/save JSON files
   - Export/import functionality

4. **Validation**
   - Detect circular dependencies
   - Detect isolated nodes
   - Detect missing prerequisites

### 6.4 File Structure

```
tools/quest_editor/
├── main.py              # Entry point
├── editor_app.py        # Main window
├── canvas_view.py       # Canvas node graph view
├── property_panel.py    # Property edit panel
├── quest_node.py        # Quest node data model
├── quest_data.py        # JSON data read/write
└── validation.py        # Validation logic
```

## 7. Branching System

### 7.1 Branch Model

Quest branching is achieved through **prerequisites + groups**:

```
                    ┌─────┐
                    │Q001 │ (Starting quest)
                    └──┬──┘
                       │
              ┌────────┴────────┐
              ▼                 ▼
         ┌─────┐           ┌─────┐
         │Q002 │           │Q003 │ (Branches)
         │农耕线│           │矿工线│
         └──┬──┘           └──┬──┘
            │                 │
            ▼                 ▼
         ┌─────┐           ┌─────┐
         │Q004 │           │Q005 │
         └──┬──┘           └──┬──┘
            │                 │
            └────────┬────────┘
                     ▼
                 ┌─────┐
                 │Q006 │ (Convergence)
                 └─────┘
```

### 7.2 Branch Configuration

```json
{
  "quests": {
    "quest_001": {
      "id": "quest_001",
      "name": "初识农耕",
      "prerequisites": [],
      "branch_group": null,
      "rewards": {
        "unlock_quests": ["quest_002", "quest_003"]
      }
    },
    "quest_002": {
      "id": "quest_002",
      "name": "番茄之路",
      "prerequisites": ["quest_001"],
      "branch_group": "farming_path",
      "rewards": {
        "unlock_quests": ["quest_004"]
      }
    },
    "quest_003": {
      "id": "quest_003",
      "name": "矿工入门",
      "prerequisites": ["quest_001"],
      "branch_group": "mining_path",
      "rewards": {
        "unlock_quests": ["quest_005"]
      }
    },
    "quest_004": {
      "id": "quest_004",
      "name": "高级农耕",
      "prerequisites": ["quest_002"],
      "branch_group": "farming_path",
      "rewards": {
        "unlock_quests": ["quest_006"]
      }
    },
    "quest_005": {
      "id": "quest_005",
      "name": "高级矿工",
      "prerequisites": ["quest_003"],
      "branch_group": "mining_path",
      "rewards": {
        "unlock_quests": ["quest_006"]
      }
    },
    "quest_006": {
      "id": "quest_006",
      "name": "全面发展",
      "prerequisites": ["quest_004", "quest_005"],
      "branch_group": null,
      "rewards": {
        "items": [{"id": "golden_hoe", "count": 1}],
        "gold": 500,
        "exp": 200
      }
    }
  },
  "branch_groups": {
    "farming_path": {
      "name": "农耕路线",
      "description": "专注于农业发展",
      "unlock_condition": "quest_001.completed",
      "quest_ids": ["quest_002", "quest_004"]
    },
    "mining_path": {
      "name": "矿工路线",
      "description": "专注于矿业发展",
      "unlock_condition": "quest_001.completed",
      "quest_ids": ["quest_003", "quest_005"]
    }
  }
}
```

### 7.3 Branch Unlock Logic

```
1. Player completes Q001
   └→ Check Q001's rewards.unlock_quests: ["Q002", "Q003"]
   └→ Q002 and Q003 become NOT_ACCEPTED (available)

2. Player accepts Q002
   └→ Q002 state changes to ACCEPTED
   └→ Q003 remains NOT_ACCEPTED (still available)

3. Player completes Q002
   └→ Q004 unlocks
   └→ Q003 still available

4. Player completes Q004 and Q005
   └→ Q006 unlocks (convergence point)
```

### 7.4 Branch Group vs Prerequisites

- **prerequisites**: List of quest IDs that must be completed before this quest becomes available. Used for linear dependencies.
- **branch_group**: Optional string that groups related quests together for UI display and logical organization. Does not affect unlock logic.

Example:
- Q002 has `prerequisites: ["quest_001"]` → Q001 must be completed before Q002 is available
- Q002 has `branch_group: "farming_path"` → Q002 is visually grouped with other farming quests

## 8. Error Handling

### 8.1 Server Error Handling

| Scenario | Handling |
|---|---|
| Quest config load failure | Log error, skip invalid quests, server continues |
| Player quest data corruption | Reset player quest data, log warning |
| Event handling exception | Catch exception, log, don't affect other events |
| Reward distribution failure | Retry 3 times, log error on failure, don't affect quest state |
| Circular dependency detection | Detect at startup, disable all quests in circular chain |

### 8.2 Editor Error Handling

| Scenario | Handling |
|---|---|
| JSON file format error | Popup with error location, preserve edit content |
| Circular dependency | Red highlight problem nodes, prevent save |
| Isolated nodes | Yellow warning, allow save but prompt |
| Missing required fields | Red border mark, prevent save |

## 9. Testing Strategy

### 9.1 Server Testing

1. **Unit Tests**
   - Quest state machine transitions
   - Event matching logic
   - Reward distribution logic
   - Progress calculation

2. **Integration Tests**
   - Quest accept → progress update → complete → reward distribution full flow
   - Branch unlock logic
   - Multiple quests parallel progress update
   - Player logout/login data persistence

### 9.2 Editor Testing

1. **Functional Tests**
   - Node create/delete/move
   - Connect/disconnect
   - Property editing
   - JSON read/write

2. **Boundary Tests**
   - Empty quest configuration
   - Large quest graph (100+ nodes)
   - Circular dependency detection
   - Special character handling

## 10. Implementation Phases

### Phase 1: Core Framework

1. **Event System (EventBus)**
   - EventBus singleton implementation
   - Event type definitions
   - Subscribe/publish mechanism

2. **Quest Data Model**
   - TaskState enum
   - TaskInfo structure
   - JSON configuration loading

3. **QuestManager Basics**
   - Player quest state management
   - Event listening and progress updates
   - Quest completion detection

### Phase 2: Rewards & Persistence

1. **Reward System**
   - Item distribution
   - Gold/exp distribution
   - Quest unlocking

2. **Data Persistence**
   - PlayerData add taskInfos field
   - Quest progress save/load
   - Auto-save mechanism

### Phase 3: Client Interaction

1. **Message Protocol**
   - Protobuf message definitions
   - Client message handling

2. **Quest UI**
   - Quest list interface
   - Quest detail interface
   - Quest progress prompts

### Phase 4: Quest Editor

1. **Basic Editor**
   - tkinter main window
   - Canvas node graph view
   - Property edit panel

2. **Advanced Features**
   - Connect/disconnect
   - Validation features
   - File operations

### Phase 5: Refinement & Optimization

1. **Quest Type Completion**
   - Dialogue quests
   - Exploration quests
   - Crafting quests

2. **Branch System Completion**
   - Branch group management
   - Complex branch logic

3. **Testing & Optimization**
   - Unit tests
   - Integration tests
   - Performance optimization
