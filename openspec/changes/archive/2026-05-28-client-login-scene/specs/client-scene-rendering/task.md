# Task: client-scene-rendering

## 1. 资源准备

### 1.1 创建 tileset 精灵图 [已完成 ✅]
- 生成 16x16 像素 tileset PNG（含地面类型: 草地/泥地/水面/沙地/翻耕，地物类型: 石头/树木/门/床/电视/炉灶）
- 存放路径: `scripts/client/assets/tilesets/farm_tileset.png`

### 1.2 创建 TMX 地图文件 [已完成 ✅]
- 使用 pytmx XML 格式创建 `scripts/client/assets/maps/farm.tmx`
- 包含图层: Ground（地面层）、Objects（地物层）
- 使用 tile 属性 walkable=false 标记不可通行瓦片
- 地图尺寸: 60x50 tiles，每 tile 16x16 像素

### 1.3 创建玩家精灵图 [已完成 ✅]
- 生成 16x16 像素的玩家精灵表（4方向 x 4帧 = 64x64 PNG）
- 行: down(0), left(1), right(2), up(3)
- 列: frame0(静止), frame1-3(行走)
- 存放路径: `scripts/client/assets/sprites/player.png`

## 2. 核心模块开发

### 2.1 修改 constants.py [已完成 ✅]
- TILE_SIZE 从 32 改为 16（匹配 spec 要求的 16x16 基础瓦片）
- 新增 ZOOM_FACTOR = 4（4x 整数缩放）
- 新增 PLAYER_ANIM_FRAME_DURATION = 0.15（行走动画帧间隔）
- 调整 PLAYER_SPEED 为 64（16px tiles，4 tiles/s）
- 调整 DEFAULT_MAP_WIDTH/HEIGHT 为 120/100（保持相同世界大小）
- 新增 ASSETS_DIR, DEFAULT_MAP_PATH, PLAYER_SPRITE_PATH 常量

### 2.2 创建 tmx_map.py 模块 [已完成 ✅]
- TmxMapLoader 类：使用 pytmx.util_pygame.load_pygame 加载 .tmx 文件
- 碰撞检测基于 tile 属性 walkable=false
- 提供 get_ground_type/get_object_type 返回枚举类型
- 提供 check_walkable_rect 四角碰撞检测

### 2.3 创建 player_sprite.py 模块 [已完成 ✅]
- PlayerSprite 类（继承 pygame.sprite.Sprite）
- 支持 4 方向行走动画（4 帧循环）
- 停止时显示静止帧
- 动画速度与移动速度匹配
- _layer 属性控制渲染层级（位于地面层之上、地物层之下）
- 支持 zoom 预缩放精灵图像

### 2.4 创建 hud.py 模块 [已完成 ✅]
- HUD 类：渲染游戏内信息覆盖层
- 显示: role_name、当前坐标 (x, y)、scene_id
- 半透明背景，位于屏幕左上角

## 3. 集成重构

### 3.1 重构 game_scene.py 使用 pyscroll [已完成 ✅]
- 替换 TileMap + TileRenderer 为 pyscroll.BufferedRenderer + pyscroll.PyscrollGroup
- 使用 TmxMapLoader 加载 .tmx 地图
- 使用 PlayerSprite 替代当前方块渲染
- 碰撞检测改用 TmxMapLoader 的 check_walkable_rect
- 保留现有网络消息处理、位置更新发送等逻辑
- 集成 HUD 显示

### 3.2 修改 camera.py 添加 lerp 平滑跟随 [已完成 ✅]
- Camera.follow() 添加 lerp 插值参数（CAMERA_LERP_FACTOR=0.1）
- 平滑跟随玩家移动，而非直接跳到玩家位置
- 保留地图边界钳制功能
- 移除对 tile_map.pixel_to_tile 的依赖

## 4. 验证

### 4.1 编译/运行验证 [待完成]
- 运行客户端确认无导入错误
- 验证地图正确加载和渲染
- 验证玩家精灵动画
- 验证碰撞检测
- 验证 HUD 显示
