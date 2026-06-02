---
name: code-split-refactor
description: 代码拆分重构策略、拆分案例、依赖注入模式。
metadata:
  type: reference
---

# 代码拆分重构

## 概述

代码拆分重构的目标是将大文件拆分为职责单一的小模块，提升可维护性。

## 架构设计

### 拆分策略

- 按职责划分：每个模块单一职责
- 协调层保留：跨模块交互逻辑放在协调层
- 依赖注入：模块间通过回调解耦

### 已完成案例

game_scene.py (877行) → 4 个模块：
- PlayerController (303行)：玩家移动/位置同步
- NetworkDispatcher (176行)：网络消息分发
- GameRenderer (120行)：渲染逻辑
- GameScene (493行)：薄协调层

## 关键流程

### 拆分步骤

1. 识别大文件中的职责边界
2. 提取独立模块
3. 定义模块接口
4. 使用依赖注入连接模块
5. 验证功能不变
6. 更新导入路径

### 依赖注入模式

```python
class PlayerController:
    def __init__(self, connection, tmx_map, player_sprite):
        self.connection = connection
        self.tmx_map = tmx_map
        self.player_sprite = player_sprite
```

## 关键代码路径

- 拆分后的模块：`scripts/client/player_controller.py`, `network_dispatcher.py`, `game_renderer.py`, `game_scene.py`
- 设计文档：`docs/superpowers/specs/2026-06-02-code-split-refactor-design.md`

## 常见陷阱

### 循环导入

模块间相互导入导致启动失败：
- 使用依赖注入而非直接导入
- 协调层持有所有模块引用

### 依赖注入遗漏

模块创建时忘记注入依赖：
- 使用构造函数注入，遗漏时报错明显
- 编写初始化测试

### 拆分后接口不一致

拆分后模块接口与原代码不兼容：
- 保持原有公共接口不变
- 使用 adapter 模式适配

## 扩展指南

### 拆分新文件

1. 分析文件职责
2. 识别可独立的模块
3. 按上述步骤执行拆分
4. 编写测试验证

## 相关 Skill

- [[client-architecture]] — 客户端架构
