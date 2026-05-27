## Purpose

物品使用与掉落系统：客户端发送使用请求，服务器验证并执行效果（改变地物/地面、产出物品、消耗物品），掉落物实体在世界中渲染并自动拾取。

## Requirements

### Requirement: 物品使用请求
客户端 SHALL 在玩家触发交互时发送 ItemUseReq 给服务器。

#### Scenario: 空格键触发
- **WHEN** 玩家按下空格键
- **THEN** 客户端发送 ItemUseReq { direction } 给服务器

#### Scenario: 鼠标点击触发
- **WHEN** 玩家鼠标左键点击世界中的 tile
- **THEN** 客户端发送 ItemUseReq { target_x, target_y } 给服务器

### Requirement: 服务器验证与执行
Game Server SHALL 验证物品使用请求并执行效果。

#### Scenario: 验证通过并执行
- **WHEN** 服务器收到 ItemUseReq，active item 匹配目标效果，能量充足
- **THEN** 执行效果（修改地物/地面、增减物品、消耗能量），返回 ItemUseResp code=0

#### Scenario: 无手持物品
- **WHEN** 服务器收到 ItemUseReq，但 activeSlot 为空
- **THEN** 检查面前是否有成熟作物可收割，无则返回 code=1

#### Scenario: 效果不匹配
- **WHEN** 服务器收到 ItemUseReq，手持斧头但面前是草地
- **THEN** 返回 code=2（无匹配效果）

#### Scenario: 能量不足
- **WHEN** 服务器收到 ItemUseReq，但能量 < energy_cost
- **THEN** 返回 code=ENERGY_EXHAUSTED

### Requirement: 效果执行
服务器 SHALL 按 ITEM_EFFECTS 配置执行效果。

#### Scenario: 移除地物（砍石头）
- **WHEN** 效果包含 `remove_object=True`
- **THEN** 将目标位置的地物设为 None

#### Scenario: 设置地面（翻耕）
- **WHEN** 效果包含 `set_ground="TILLED"`
- **THEN** 将目标位置的地面设为 TILLED

#### Scenario: 放置地物（播种）
- **WHEN** 效果包含 `place_object="CROP_GROWING"`
- **THEN** 在目标位置放置 CROP_GROWING 地物，注册到 CropSystem

#### Scenario: 消耗物品
- **WHEN** 效果包含 `consume_self=True`
- **THEN** 从背包中移除 1 个当前手持物品

#### Scenario: 掉落物产出
- **WHEN** 效果包含 `drops=[{"id": 2, "min": 1, "max": 3}]`
- **THEN** 生成 1~3 个石头掉落到目标位置附近

### Requirement: 收割成熟作物
玩家 SHALL 能空手收割面前的成熟作物。

#### Scenario: 收割成功
- **WHEN** 面前 tile 的地物为 CROP_READY，玩家按下空格
- **THEN** 移除 CROP_READY 地物，地面保持 TILLED，背包获得 2~3 个作物

#### Scenario: 收割距离
- **WHEN** 玩家与 CROP_READY 地物的切比雪夫距离 > 1
- **THEN** 不能收割

### Requirement: 掉落物生成
服务器 SHALL 在效果产出物品时生成掉落物实体。

#### Scenario: 掉落物生成位置
- **WHEN** 石头被敲碎，产出 2 个石头
- **THEN** 在目标 tile 附近随机偏移位置生成 2 个 DropItem 实体

#### Scenario: 掉落物生命周期
- **WHEN** DropItem 存在超过 300 秒
- **THEN** 服务器移除该 DropItem，通知客户端

### Requirement: 掉落物渲染
客户端 SHALL 在世界中渲染掉落物实体。

#### Scenario: 掉落物显示
- **WHEN** 服务器下发 DropItemSync（spawn）
- **THEN** 客户端在对应世界坐标绘制物品图标

#### Scenario: 浮动动画
- **WHEN** 掉落物在世界中
- **THEN** 物品图标上下浮动（sin 波动，幅度 2px）

#### Scenario: 掉落物消失
- **WHEN** 服务器下发 DropItemSync（remove）
- **THEN** 客户端移除对应掉落物的渲染

### Requirement: 自动拾取
玩家靠近掉落物时 SHALL 自动拾取入背包。

#### Scenario: 靠近拾取
- **WHEN** 玩家与 DropItem 的像素距离 < 48px
- **THEN** 服务器将物品加入背包，移除 DropItem，发送 InventorySync 和 DropItemSync(pickup)

#### Scenario: 背包满时保留
- **WHEN** 玩家靠近 DropItem 但背包已满
- **THEN** DropItem 保留在地面上，不被拾取

### Requirement: 物品使用冷却
物品使用 SHALL 有冷却时间，防止过快连续使用。

#### Scenario: 冷却时间
- **WHEN** 玩家使用物品后
- **THEN** 300ms 内不能再次使用物品

#### Scenario: 冷却期间按键
- **WHEN** 冷却期间玩家按空格
- **THEN** 忽略该输入
