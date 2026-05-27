# Task: portal-system

## 1. Portal 数据结构定义
- [已完成] Portal 字段: pos(x,y), trigger_dir, target_scene, target_portal_id
  - 文件: `scripts/client/scene/scene_defs.py` SCENE_DEFS 字典
  - Farm portals: (25,48) 和 (26,48), trigger_dir="down", target="house"
  - House portal: (5,7), trigger_dir="down", target="farm"
- [已完成] Farm 场景 Portal: 位置在房屋入口，target_scene="house"
- [已完成] House 场景 Portal: 位置在门口，target_scene="farm"

## 2. Portal 可通行
- [已完成] DOOR_IN walkable=True
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES[ObjectType.DOOR_IN]
- [已完成] DOOR_OUT walkable=True
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES[ObjectType.DOOR_OUT]

## 3. 方向触发检测
- [已完成] check_portal_trigger 方法实现
  - 文件: `scripts/client/scene/scene_manager.py` check_portal_trigger()
  - 检查当前位置地物是否为 portal 类型，再调用 get_portal_at 匹配位置+方向
- [已完成] get_portal_at 函数实现
  - 文件: `scripts/client/scene/scene_defs.py` get_portal_at()
  - 匹配 tile 位置和 trigger_dir
- [已完成] GameScene._check_portal_trigger 在移动后调用
  - 文件: `scripts/client/game_scene.py` _check_portal_trigger()

## 4. 空格/鼠标交互触发
- [已完成] 空格键触发 Portal 切换
  - 文件: `scripts/client/game_scene.py` _handle_portal_interaction()
  - 玩家站在 Portal 旁，面前 tile 是 Portal，按空格键触发场景切换
  - 检查 interact action 按下 + 过渡状态屏蔽 + 朝向计算面前 tile + is_portal 检测
- [已完成] 鼠标点击 Portal 触发场景切换
  - 文件: `scripts/client/game_scene.py` _handle_mouse_click()
  - 鼠标左键点击 Portal tile，检测 interact_type='portal'，调用 request_scene_change
  - 通过 interaction.is_portal() 和 get_portal_scene() 检测

## 5. Portal 与地物类型关联
- [已完成] DOOR_IN interact_type='portal'
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES[ObjectType.DOOR_IN]
- [已完成] DOOR_OUT interact_type='portal'
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES[ObjectType.DOOR_OUT]

## 6. 目标位置映射
- [已完成] get_spawn_for_portal 函数
  - 文件: `scripts/client/scene/scene_defs.py` get_spawn_for_portal()
  - 根据目标 portal 的 trigger_dir 偏移计算出生点
- [已完成] SceneManager.handle_scene_change_resp 处理服务器响应
  - 文件: `scripts/client/scene/scene_manager.py`
  - 返回 spawn 像素坐标
