# Task: 地图双层模型实现

## 任务拆分

### 1. 添加 ObjectType 枚举和地物属性到 constants.py
- [x] 1.1 定义 ObjectType 枚举（NONE=0, STONE=1, CROP_GROWING=2, CROP_READY=3, TREE=4, DOOR_IN=10, DOOR_OUT=11, BED=12, TV=13, STOVE=14）[已完成 ✅]
- [x] 1.2 定义 OBJECT_PROPERTIES 字典（包含 walkable, interactable, interact_type, sprite_data）[已完成 ✅]
- [x] 1.3 定义 16×16 像素精灵数据（每种 ObjectType 的颜色和形状）[已完成 ✅]

### 2. 更新 TileMap 支持双层数据结构
- [x] 2.1 添加 `_objects` 二维数组（默认填充 NONE）[已完成 ✅]
- [x] 2.2 添加 `get_object(tx, ty)` 方法 [已完成 ✅]
- [x] 2.3 添加 `set_object(tx, ty, obj_type)` 方法 [已完成 ✅]
- [x] 2.4 更新 `fill_from_data()` 支持 ground + objects 双层数据 [已完成 ✅]

### 3. 实现地物精灵系统
- [x] 3.1 创建 ObjectSpriteManager 类（精灵缓存管理）[已完成 ✅]
- [x] 3.2 实现 16×16 像素精灵生成（使用 PyGame Surface）[已完成 ✅]
- [x] 3.3 实现精灵拉伸到 TILE_SIZE（NEAREST 采样）[已完成 ✅]
- [x] 3.4 实现透明通道支持（alpha 通道）[已完成 ✅]

### 4. 更新 TileRenderer 支持分层渲染
- [x] 4.1 修改 render() 方法实现两遍渲染 [已完成 ✅]
- [x] 4.2 第一遍：渲染地面层（现有逻辑）[已完成 ✅]
- [x] 4.3 第二遍：渲染地物层（使用 ObjectSpriteManager）[已完成 ✅]
- [x] 4.4 集成 ObjectSpriteManager 到 TileRenderer [已完成 ✅]

### 5. 更新 GameScene 支持双层地图数据
- [x] 5.1 更新 _handle_map_data_notify() 解析双层数据 [已完成 ✅]
- [x] 5.2 更新 _generate_default_map() 生成示例地物 [已完成 ✅]
- [x] 5.3 更新 _check_walkable() 考虑地物层碰撞 [已完成 ✅]

### 6. 添加交互双层匹配逻辑
- [x] 6.1 创建 interaction.py 模块 [已完成 ✅]
- [x] 6.2 实现 get_interaction_key(tile_map, tx, ty) 函数 [已完成 ✅]
- [x] 6.3 定义 ITEM_EFFECTS 匹配表（示例）[已完成 ✅]
- [x] 6.4 实现 match_item_effect(item, tile_map, tx, ty) 函数 [已完成 ✅]

## 依赖关系
- 任务 1-3 是基础，无依赖
- 任务 4 依赖任务 2 和 3
- 任务 5 依赖任务 2
- 任务 6 依赖任务 2
