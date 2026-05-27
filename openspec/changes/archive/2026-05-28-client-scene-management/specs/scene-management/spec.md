## Purpose

场景管理框架：在客户端管理多个场景的渲染切换，支持场景注册表、场景切换流程（含 Iris 过渡动效），以及服务端的场景数据管理和冻结/恢复机制。

## Requirements

### Requirement: 场景注册表
系统 SHALL 在 `scene_defs.py` 中注册所有场景的配置。

#### Scenario: 场景定义
- **WHEN** 系统初始化
- **THEN** 注册至少两个场景：farm（60×50）和 house（10×8）

#### Scenario: 场景定义内容
- **WHEN** 查询 SCENE_DEFS["farm"]
- **THEN** 返回包含 width, height, generate(), portals[], player_spawn 的配置

#### Scenario: 地图生成函数
- **WHEN** 调用 SCENE_DEFS["farm"]["generate"]()
- **THEN** 返回 {ground[][], objects[][]}，可直接构建 TileMap

### Requirement: 客户端 SceneManager
SceneManager SHALL 管理当前场景的渲染和切换。

#### Scenario: 初始化当前场景
- **WHEN** 玩家登录成功，服务器下发 active_scene="farm"
- **THEN** SceneManager 加载 farm 场景的 TileMap，开始渲染

#### Scenario: 切换场景
- **WHEN** 收到 SceneChangeResp {target_scene="house", spawn_x=5, spawn_y=6}
- **THEN** SceneManager 替换当前 TileMap 为 house 场景，更新玩家位置

#### Scenario: 未知场景
- **WHEN** 收到的 target_scene 在 SCENE_DEFS 中不存在
- **THEN** 记录错误日志，保持当前场景不变

### Requirement: 服务端场景数据管理
Game Server SHALL 维护每个场景的独立数据。

#### Scenario: 场景数据隔离
- **WHEN** 玩家 A 在 farm 场景，玩家 B 在 house 场景
- **THEN** 两个场景的 TileMap、CropSystem、DropItems 互不影响

#### Scenario: 场景首次加载
- **WHEN** 玩家进入一个从未有人进入的场景
- **THEN** 服务器调用 SceneDef.generate() 创建默认地图数据

#### Scenario: 场景数据持久化
- **WHEN** 服务器关闭或场景数据变更
- **THEN** 场景数据保存到 DBMgr

### Requirement: 场景切换请求
客户端 SHALL 在 Portal 触发时发送 SceneChangeReq 给服务器。

#### Scenario: Portal 触发发送请求
- **WHEN** 玩家在 Portal tile 上按触发方向移动
- **THEN** 客户端发送 SceneChangeReq {target_scene, target_portal_id}

#### Scenario: 服务器处理切换
- **WHEN** 服务器收到 SceneChangeReq
- **THEN** 验证 Portal 合法性，冻结当前场景，加载目标场景，设置玩家位置，返回 SceneChangeResp

#### Scenario: 切换失败
- **WHEN** 目标场景不存在或 Portal 非法
- **THEN** 服务器返回 SceneChangeResp code=失败

### Requirement: 服务端冻结/恢复
Game Server SHALL 在场景无人时冻结，有人进入时恢复。

#### Scenario: 冻结场景
- **WHEN** 最后一个玩家离开某场景
- **THEN** 记录 frozen_at 时间戳，暂停该场景的 CropSystem 更新

#### Scenario: 恢复场景
- **WHEN** 玩家进入已冻结的场景
- **THEN** 计算 elapsed = now - frozen_at，调用 crop_system.simulate_elapsed(elapsed) 补帧

#### Scenario: 补帧作物成熟
- **WHEN** 场景冻结了 120 秒，某作物在冻结前 30 秒种植
- **THEN** 补帧后该作物总经过 150 秒 > 60 秒，立即成熟

### Requirement: 输入屏蔽
场景切换过渡期间 SHALL 屏蔽所有游戏输入。

#### Scenario: 过渡期间屏蔽输入
- **WHEN** SceneTransition 状态非 IDLE
- **THEN** SceneManager 不处理任何游戏输入（移动、交互、鼠标点击）

#### Scenario: 过渡完成后恢复
- **WHEN** SceneTransition 状态回到 IDLE
- **THEN** 恢复正常输入处理

### Requirement: 存档格式升级
场景数据 SHALL 从单场景升级为多场景结构。

#### Scenario: 新存档格式
- **WHEN** 场景数据被序列化
- **THEN** 格式为 `{"active_scene": "farm", "scenes": {"farm": {...}, "house": {...}}}`

#### Scenario: 旧存档迁移
- **WHEN** 加载旧格式存档（单场景）
- **THEN** 包装为 `scenes.farm`，`active_scene = "farm"`
