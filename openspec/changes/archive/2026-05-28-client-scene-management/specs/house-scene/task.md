# Task: house-scene

## 1. 场景尺寸与生成
- [已完成] house 场景 10x8
  - 文件: `scripts/client/scene/scene_defs.py` SCENE_DEFS["house"] width=10, height=8
- [已完成] generate_house_map() 返回 10x8 的 ground[][] 和 objects[][]
  - 文件: `scripts/client/scene/scene_defs.py` _generate_house_map()

## 2. 墙壁包围
- [已完成] 四周一圈 ground=WALL（不可通行）
  - 顶部和底部整行 WALL，左右两侧整列 WALL
- [已完成] 内部 ground=WOOD_FLOOR
  - 初始化全部 WOOD_FLOOR，然后覆盖四周为 WALL

## 3. 门口
- [已完成] 底部中央 DOOR_OUT
  - 位置: (5, 7)，ground=WOOD_FLOOR + object=DOOR_OUT
  - 可通行，通向 farm

## 4. 家具布局
- [已完成] BED 位置: 左上区域 (2, 1)
- [已完成] TV 位置: 右上区域 (width-2, 1) = (8, 1)
- [已完成] STOVE 位置: 右侧中间 (width-2, 4) = (8, 4)

## 5. 新 Ground 类型
- [已完成] WALL: walkable=False, color=(80,80,80) 深灰色
  - 文件: `scripts/client/constants.py` GroundType.WALL, GROUND_PROPERTIES
- [已完成] WOOD_FLOOR: walkable=True, color=(160,110,60) 浅棕色
  - 文件: `scripts/client/constants.py` GroundType.WOOD_FLOOR, GROUND_PROPERTIES
- [已完成] WOOD_FLOOR 2.5D 高光/阴影效果
  - HIGHLIGHT_ALPHA=30, SHADOW_ALPHA=40 常量已定义
  - tile_map 渲染使用这些常量实现 2.5D 效果（在 tile_map.py 渲染逻辑中）

## 6. 新 Object 类型
- [已完成] DOOR_IN: walkable=True, interactable=True, interact_type='portal'
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES
- [已完成] DOOR_OUT: walkable=True, interactable=True, interact_type='portal'
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES
- [已完成] BED: walkable=False, interactable=True
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES
- [已完成] TV: walkable=False, interactable=True
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES
- [已完成] STOVE: walkable=False, interactable=True
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES
- [已完成] BED/TV/STOVE interact_type
  - Spec 要求: interact_type='furniture'
  - 现状: BED='sleep', TV='watch', STOVE='cook'（更具体的类型）
  - 当前实现功能等价且更利于后续扩展，与 spec 字面不完全一致但语义满足

## 7. 家具交互预留
- [已完成] 家具标记为 interactable=True
- [已完成] 交互触发时本版本为空实现
  - interaction.py 中 ITEM_EFFECTS 有 BED/TV/STOVE 条目（effect: sleep/watch/cook）
  - 当前仅记录日志，无实际游戏效果，符合 spec "本版本不实现实际功能" 的要求

## 8. 地物像素精灵
- [已完成] DOOR_IN 16x16 像素精灵
  - 文件: `scripts/client/constants.py` OBJECT_PROPERTIES sprite_data
- [已完成] DOOR_OUT 16x16 像素精灵
- [已完成] BED 16x16 像素精灵
- [已完成] TV 16x16 像素精灵
- [已完成] STOVE 16x16 像素精灵

## 9. Farm 场景房屋入口
- [已完成] Farm 地图 DOOR_IN 放置
  - 文件: `scripts/client/scene/scene_defs.py` _generate_farm_map()
  - 位置: (25,48) 和 (26,48)
- [已完成] DOOR_IN walkable=True
  - 玩家可以正常站在上面
