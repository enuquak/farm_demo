---
name: npc-dialog
description: NPC 对话系统架构、DialogTree、对话触发、客户端 UI。
metadata:
  type: reference
---

# NPC 对话系统

## 概述

NPC 对话系统支持树状对话结构，玩家与 NPC 交互时触发对话，选择选项推进对话流程。与任务系统深度集成。

## 架构设计

### 对话树结构

```
DialogTree
├── DialogNode 1（NPC 对话）
│   ├── DialogOption A → DialogNode 2
│   └── DialogOption B → DialogNode 3
├── DialogNode 2（NPC 对话）
│   └── DialogOption → close
└── DialogNode 3（玩家选择）
    ├── Option X → DialogNode 4
    └── Option Y → close
```

### 对话效果

- 跳转节点：goto(node_id)
- 触发任务：start_quest(quest_id)
- 给予物品：give_item(item_id, count)
- 关闭对话：close()

## 关键流程

### 对话触发

1. 玩家站在 NPC 附近（距离 ≤ 1 格）
2. 按交互键或点击 NPC
3. 客户端发送 NpcInteractReq（npc_id）
4. 服务端返回对话树数据
5. 客户端渲染 DialogPanel

### 选项选择

1. 玩家点击对话选项
2. 客户端发送 DialogOptionReq（npc_id, option_id）
3. 服务端执行选项效果
4. 返回下一步对话内容或关闭对话

## 关键代码路径

- NPC 管理：`scripts/server/game_server/src/npc_manager.h/cpp`
- 对话树定义：`config/dialogs/` 目录
- 客户端 UI：`scripts/client/ui/dialog_panel.py`
- Protobuf：`scripts/common/proto/npc.proto`

## 常见陷阱

### 对话树循环引用

对话节点形成死循环：
- 加载时检测循环引用
- 设置最大跳转次数限制

### NPC 位置同步

客户端和服务端 NPC 位置不一致：
- NPC 位置在场景配置中定义
- 客户端加载场景时同步 NPC 位置

### 对话状态与任务状态不一致

对话中显示的任务状态与实际不符：
- 对话打开时重新查询任务状态
- 任务状态变更时刷新对话 UI

## 扩展指南

### 添加新 NPC

1. 在场景配置中定义 NPC 位置
2. 在 `config/dialogs/` 中创建对话树文件
3. 在 NPCManager 中注册 NPC

## 相关 Skill

- [[quest-system]] — 任务系统（对话触发任务）
- [[scene-system]] — 场景管理（NPC 位置）
