## Purpose

游戏内时钟核心逻辑：在服务端以 30 分钟为粒度推进游戏时间（现实 1 分钟 = 游戏 30 分钟），管理日/时/分计算、暂停/恢复，以及 slot 事件回调。

## Requirements

### Requirement: 时间模型
GameClock SHALL 使用离散 time_slot 整数 (0~39) 表示当天进度。

#### Scenario: 时间映射
- **WHEN** time_slot=0
- **THEN** 对应 AM 6:00

#### Scenario: 中午
- **WHEN** time_slot=12
- **THEN** 对应 PM 12:00

#### Scenario: 午夜
- **WHEN** time_slot=36
- **THEN** 对应 AM 0:00（即 24:00）

#### Scenario: 一天结束
- **WHEN** time_slot=40
- **THEN** 触发 on_day_end 回调

### Requirement: 时间推进
GameClock SHALL 每 60 秒现实时间推进 1 个 time_slot（30 分钟游戏时间）。

#### Scenario: 正常推进
- **WHEN** 调用 update(dt) 且 dt=60.0 秒
- **THEN** time_slot 增加 1

#### Scenario: 累积推进
- **WHEN** 调用 update(dt) 且 dt=45.0 秒，elapsed 已累积 30.0 秒
- **THEN** elapsed 更新为 75.0 秒，time_slot 增加 1，elapsed 减去 60.0 秒变为 15.0

#### Scenario: 帧率无关
- **WHEN** 以 30 FPS 和 60 FPS 分别运行
- **THEN** 相同现实时间内 time_slot 推进相同

### Requirement: 时间显示
GameClock SHALL 提供 display_time 属性返回格式化的时间字符串。

#### Scenario: 早晨显示
- **WHEN** time_slot=0
- **THEN** display_time = "AM 6:00"

#### Scenario: 下午显示
- **WHEN** time_slot=20
- **THEN** display_time = "PM 4:00"

#### Scenario: 凌晨显示
- **WHEN** time_slot=38
- **THEN** display_time = "AM 1:00"

### Requirement: 天数管理
GameClock SHALL 维护 day 计数器。

#### Scenario: 初始天数
- **WHEN** 新游戏开始
- **THEN** day=1

#### Scenario: 天数递增
- **WHEN** on_day_end 触发后新的一天开始
- **THEN** day 增加 1, time_slot 重置为 0

### Requirement: 暂停与恢复
GameClock SHALL 支持暂停和恢复。

#### Scenario: 暂停
- **WHEN** 调用 clock.pause()
- **THEN** 后续 update(dt) 不推进 time_slot

#### Scenario: 恢复
- **WHEN** 调用 clock.resume()
- **THEN** 后续 update(dt) 正常推进 time_slot

#### Scenario: 暂停时 elapsed 保留
- **WHEN** 暂停前 elapsed=30.0
- **THEN** 恢复后 elapsed 仍为 30.0，继续累积

### Requirement: 回调机制
GameClock SHALL 在 slot 跳变和一天结束时触发回调。

#### Scenario: 新 slot 回调
- **WHEN** time_slot 从 5 变为 6
- **THEN** 调用 on_new_slot(6) 回调

#### Scenario: 一天结束回调
- **WHEN** time_slot 达到 40
- **THEN** 调用 on_day_end() 回调

### Requirement: 时钟同步
服务器 SHALL 在每个 slot 跳变时广播 ClockSync 给所有在线玩家。

#### Scenario: 广播时钟
- **WHEN** time_slot 从 5 变为 6
- **THEN** 服务器发送 ClockSync {day, time_slot, paused} 给所有客户端

#### Scenario: 客户端更新
- **WHEN** 客户端收到 ClockSync
- **THEN** 更新本地时钟显示

### Requirement: 序列化与反序列化
GameClock SHALL 支持序列化和反序列化。

#### Scenario: 序列化
- **WHEN** 调用 clock.serialize()
- **THEN** 返回 {"day": 1, "time_slot": 5, "elapsed": 30.0}

#### Scenario: 反序列化
- **WHEN** 调用 GameClock.deserialize(data)
- **THEN** 恢复 day、time_slot、elapsed 状态

#### Scenario: 旧存档兼容
- **WHEN** 存档中无 clock 字段
- **THEN** 默认 day=1, time_slot=0, elapsed=0.0（第 1 天 AM 6:00）

### Requirement: 持久化
时钟数据 SHALL 纳入 DBMgr 持久化。

#### Scenario: 保存
- **WHEN** 玩家下线或服务器关闭
- **THEN** clock.serialize() 数据写入 DBMgr

#### Scenario: 加载
- **WHEN** 玩家登录
- **THEN** 从 DBMgr 加载 clock 数据恢复状态
