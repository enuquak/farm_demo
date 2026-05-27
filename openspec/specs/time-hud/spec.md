## Purpose

左上角时间显示 UI：基于 PyGame 在屏幕左上角渲染当前游戏时间和天数，根据服务器推送的 ClockSync 消息更新显示。

## Requirements

### Requirement: 时间显示位置
TimeHUD SHALL 在屏幕左上角显示时间信息。

#### Scenario: HUD 位置
- **WHEN** 游戏运行中
- **THEN** 屏幕左上角 (8, 8) 位置显示时间文字

#### Scenario: 半透明背景
- **WHEN** TimeHUD 渲染
- **THEN** 文字下方有半透明黑色背景，提高可读性

### Requirement: 时间显示格式
TimeHUD SHALL 显示当前时间和天数。

#### Scenario: 标准显示
- **WHEN** day=1, time_slot=0
- **THEN** 显示 "AM 6:00  Day 1"

#### Scenario: 下午显示
- **WHEN** day=3, time_slot=20
- **THEN** 显示 "PM 4:00  Day 3"

#### Scenario: 字体大小
- **WHEN** TimeHUD 渲染
- **THEN** 使用 24px 字体大小

### Requirement: 数据更新
TimeHUD SHALL 根据 ClockSync 消息更新显示。

#### Scenario: 收到 ClockSync
- **WHEN** 客户端收到 ClockSync {day=2, time_slot=10}
- **THEN** TimeHUD 更新显示为 "AM 11:00  Day 2"

#### Scenario: 初始显示
- **WHEN** 玩家登录成功，尚未收到 ClockSync
- **THEN** 显示默认值 "AM 6:00  Day 1"

### Requirement: 文字颜色
TimeHUD SHALL 使用白色文字渲染。

#### Scenario: 文字颜色
- **WHEN** TimeHUD 渲染文字
- **THEN** 文字颜色为白色 (255, 255, 255)

#### Scenario: 背景颜色
- **WHEN** TimeHUD 渲染背景
- **THEN** 背景颜色为半透明黑色 (0, 0, 0, 128)
