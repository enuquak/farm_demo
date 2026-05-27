## Purpose

作物生长系统：在服务端管理种植中的作物，从 TILLED → CROP_GROWING → CROP_READY 的生命周期，由服务端计时器驱动成熟，支持场景冻结/恢复时的补帧。

## Requirements

### Requirement: 作物种植
系统 SHALL 在种子播撒到翻耕土地时注册作物生长。

#### Scenario: 播种
- **WHEN** 种子使用效果触发，目标 tile 地面为 TILLED
- **THEN** 在目标位置放置 CROP_GROWING 地物，CropSystem 注册该 tile 的生长数据

#### Scenario: 生长数据
- **WHEN** 作物被种植
- **THEN** 记录 `{planted_at: timestamp, grow_time: 60}`（60 秒成熟）

#### Scenario: 不可种植的地面
- **WHEN** 种子使用在非 TILLED 地面上
- **THEN** 效果不触发，返回错误

### Requirement: 作物生长计时
CropSystem SHALL 在服务端持续检查作物是否成熟。

#### Scenario: 更新检查
- **WHEN** CropSystem.update() 被调用
- **THEN** 遍历所有 growing_tiles，检查 `now - planted_at >= grow_time`

#### Scenario: 作物成熟
- **WHEN** 某个作物的 `now - planted_at >= grow_time`
- **THEN** 将该位置的地物从 CROP_GROWING 更新为 CROP_READY，从 growing_tiles 中移除

#### Scenario: 未成熟作物不变
- **WHEN** 某个作物的 `now - planted_at < grow_time`
- **THEN** 地物保持 CROP_GROWING 不变

### Requirement: 成熟时间
作物成熟时间 SHALL 为 60 秒（实时）。

#### Scenario: 成熟时间配置
- **WHEN** 作物被种植
- **THEN** grow_time 默认为 60 秒

#### Scenario: 存档恢复时重新计算
- **WHEN** 加载存档，某个作物的 planted_at=1000，grow_time=60，当前时间=1080
- **THEN** 该作物已超过 grow_time，立即成熟为 CROP_READY

### Requirement: 场景冻结/恢复
CropSystem SHALL 支持场景冻结时暂停生长，恢复时补帧。

#### Scenario: 冻结场景
- **WHEN** 玩家离开当前场景
- **THEN** 记录 frozen_at 时间戳，CropSystem 暂停更新

#### Scenario: 恢复场景
- **WHEN** 玩家回到已冻结的场景
- **THEN** 计算 elapsed = now - frozen_at，调用 `crop_system.simulate_elapsed(elapsed)` 补帧

#### Scenario: 补帧成熟
- **WHEN** 场景冻结了 120 秒，其中有作物 planted_at 在冻结前 30 秒
- **THEN** 该作物总经过 150 秒 > 60 秒，补帧时立即成熟

### Requirement: 序列化与反序列化
CropSystem SHALL 支持序列化和反序列化。

#### Scenario: 序列化
- **WHEN** 调用 `crop_system.serialize()`
- **THEN** 返回 `{"tiles": {"30,20": {"planted_at": 1743678050, "grow_time": 60}, ...}}`

#### Scenario: 反序列化
- **WHEN** 调用 `CropSystem.deserialize(data)`
- **THEN** 恢复所有 growing_tiles 数据

#### Scenario: 旧存档兼容
- **WHEN** 存档数据中无 crops 字段
- **THEN** 使用空的 CropSystem 初始化

### Requirement: 地物与作物系统联动
地物变更 SHALL 与 CropSystem 保持同步。

#### Scenario: 播种时注册
- **WHEN** 地物被设置为 CROP_GROWING
- **THEN** CropSystem 自动注册该 tile 的生长数据

#### Scenario: 收割时注销
- **WHEN** CROP_READY 地物被收割移除
- **THEN** CropSystem 移除该 tile 的数据（如果存在）

#### Scenario: 地物被其他方式移除
- **WHEN** CROP_GROWING 地物被非正常方式移除
- **THEN** CropSystem 在下次 update 时清理无效数据
