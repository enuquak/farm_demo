# Task: scene-management

## 1. Proto 定义：场景切换消息 [已完成 ✅]

### 1.1 在 player.proto 中添加 SceneChangeReq 和 SceneChangeResp 消息 [已完成 ✅]
- SceneChangeReq: target_scene(string), target_portal_id(string)
- SceneChangeResp: code(int32), msg(string), target_scene(string), spawn_x(int32), spawn_y(int32), active_scene(string)

### 1.2 在 msg_ids.py 和 msg_ids.h 中添加场景切换消息 ID [已完成 ✅]
- MSG_ID_SCENE_CHANGE_REQ = 2201
- MSG_ID_SCENE_CHANGE_RESP = 2202

### 1.3 重新生成 protobuf Python 代码 [已完成 ✅]
- 运行 protoc 生成 player_pb2.py

## 2. 客户端常量扩展 [已完成 ✅]

### 2.1 在 constants.py 中添加新的 GroundType 和 ObjectType [已完成 ✅]
- GroundType: WALL = 5, WOOD_FLOOR = 6
- 添加对应的 GROUND_PROPERTIES

### 2.2 更新 OBJECT_PROPERTIES 中 DOOR_IN/DOOR_OUT 的 interact_type [已完成 ✅]
- 确保 interact_type = "portal"

## 3. 场景注册表 scene_defs.py [已完成 ✅]

### 3.1 创建 scripts/client/scene/ 包和 __init__.py [已完成 ✅]

### 3.2 创建 scene_defs.py 注册 farm 和 house 场景 [已完成 ✅]
- SCENE_DEFS 字典: farm(60x50), house(10x8)
- 每个场景包含: width, height, generate(), portals[], player_spawn
- farm generate() 复用现有地图生成逻辑（草地+石头+树+耕地）
- house generate() 生成室内地图（木地板+墙壁+家具）
- portals 定义门的位置和触发方向

## 4. Iris 过渡动效 scene_transition.py [已完成 ✅]

### 4.1 创建 SceneTransition 类 [已完成 ✅]
- 状态机: IDLE -> IRIS_CLOSE(300ms) -> SWITCHING(1帧) -> IRIS_OPEN(300ms) -> IDLE
- start(player_screen_pos), update(dt), apply(surface)
- 使用 PyGame Surface SRCALPHA 实现圆形遮罩

## 5. 客户端 SceneManager scene_manager.py [已完成 ✅]

### 5.1 创建 SceneManager 类 [已完成 ✅]
- 持有当前场景 TileMap 引用
- load_scene(scene_id): 加载场景 TileMap
- get_current_scene_id(): 返回当前场景 ID
- update(dt): 更新过渡动效
- is_transitioning(): 返回是否在过渡中
- render(screen): 渲染当前场景 + 过渡遮罩

### 5.2 Portal 检测逻辑 [已完成 ✅]
- check_portal_trigger(tile_x, tile_y, direction): 检查当前位置是否触发 Portal
- 发送 SceneChangeReq 给服务器

### 5.3 场景切换处理 [已完成 ✅]
- handle_scene_change_resp(payload): 处理服务器响应
- 更新 TileMap、玩家位置
- 触发 Iris 过渡动效

### 5.4 输入屏蔽 [已完成 ✅]
- is_input_blocked(): 过渡期间返回 True
- GameScene 在过渡期间跳过输入处理

## 6. 服务端场景数据管理 [已完成 ✅]

### 6.1 创建 ServerScene 结构 (C++) [已完成 ✅]
- 包含 WorldState, CropSystem, DropItemManager
- frozen_at 时间戳 (0 = 活跃)
- freeze()/thaw() 方法

### 6.2 修改 GameServer 支持多场景 [已完成 ✅]
- std::unordered_map<std::string, ServerScene> scenes_
- get_or_create_scene(scene_id): 获取或创建场景
- 场景首次加载时调用 generate_default()

### 6.3 冻结/恢复机制 [已完成 ✅]
- 最后一个玩家离开时 freeze()
- 玩家进入时 thaw() + crop_system.simulate_elapsed()

## 7. 场景切换请求处理 [已完成 ✅]

### 7.1 服务端处理 SceneChangeReq [已完成 ✅]
- 验证 Portal 合法性
- 冻结当前场景
- 加载目标场景
- 设置玩家位置
- 返回 SceneChangeResp

### 7.2 注册消息 handler [已完成 ✅]
- 在 GameServer 中注册 MSG_ID_SCENE_CHANGE_REQ handler

## 8. 存档格式升级 [已完成 ✅]

### 8.1 服务端多场景存档 [已完成 ✅]
- 格式: {"active_scene": "farm", "scenes": {"farm": {...}, "house": {...}}}
- 旧存档迁移: 单场景 -> scenes.farm

## 10. 缺陷修复 [已完成 ✅]

### 10.1 BUG-001: CMakeLists.txt 添加 scene_state.cpp [已完成 ✅]
- 在 SOURCES 列表中添加 src/scene_state.cpp（在 src/crop_system.cpp 之后）
- 修复 LNK2019 链接错误

### 10.2 BUG-002: 场景数据持久化 [已完成 ✅]
- 在 GameServer 中添加 save_all_scenes/save_scene_data/load_scene_data 方法
- handle_shutdown 时调用 save_all_scenes 保存所有场景数据到 DBMgr
- get_or_create_scene 时调用 load_scene_data 从 DBMgr 加载已保存的场景数据
- 使用 player_id=0 (SCENE_DATA_PLAYER_ID) + key="scene:<scene_id>" 存储

### 10.3 BUG-003: 存档格式升级 [已完成 ✅]
- 在 load_scene_data 中实现旧格式迁移逻辑
- 旧格式（单场景）: {"width":..., "ground":[...], "objects":[...]} -> 直接作为当前场景数据
- 新格式（多场景）: {"active_scene":"farm", "scenes":{"farm":{...}}} -> 从 scenes 字段提取

## 9. 集成与联调 [已完成 ✅]

### 9.1 修改 GameScene 使用 SceneManager [已完成 ✅]
- 替换直接 TMX 加载为 SceneManager 管理
- 集成输入屏蔽

### 9.2 修改 interaction.py 支持 Portal 触发 [已完成 ✅]
- "enter_portal"/"exit_portal" 效果发送 SceneChangeReq

### 9.3 C++ 编译验证 [已完成 ✅]
- 编译 game_server 确保无错误
