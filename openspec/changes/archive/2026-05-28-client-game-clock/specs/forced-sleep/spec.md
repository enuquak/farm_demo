## Purpose

强制睡觉流程：当游戏时间到达 AM 2:00（time_slot=40）时，触发强制睡觉流程——Iris 黑屏过渡、传送至 House 床旁、体力恢复 50%、新的一天开始。

## Requirements

### Requirement: 触发条件
强制睡觉 SHALL 在 time_slot 达到 40 时自动触发。

#### Scenario: 时间到达 AM 2:00
- **WHEN** GameClock 的 time_slot 从 39 变为 40
- **THEN** 触发 on_day_end() 回调

#### Scenario: 仅触发一次
- **WHEN** on_day_end() 已触发，time_slot 仍为 40
- **THEN** 不重复触发

### Requirement: 服务端处理
Game Server SHALL 在 on_day_end 时编排强制睡觉流程。

#### Scenario: 暂停时钟
- **WHEN** on_day_end 触发
- **THEN** 立即调用 clock.pause()

#### Scenario: 通知客户端
- **WHEN** on_day_end 触发
- **THEN** 发送 ForceSleepNotify 给当前玩家

#### Scenario: 等待客户端就绪
- **WHEN** 服务器发送 ForceSleepNotify 后
- **THEN** 等待客户端回复 ForceSleepReady

### Requirement: 客户端过渡动画
客户端 SHALL 在收到 ForceSleepNotify 时播放 Iris 黑屏过渡。

#### Scenario: 收到 ForceSleepNotify
- **WHEN** 客户端收到 ForceSleepNotify
- **THEN** 启动 Iris 收缩过渡（300ms），屏蔽所有输入

#### Scenario: 过渡完成
- **WHEN** Iris 收缩完成（radius=0）
- **THEN** 发送 ForceSleepReady 给服务器

### Requirement: 服务端场景切换
Game Server SHALL 在收到 ForceSleepReady 时执行场景切换和状态重置。

#### Scenario: 切换到 House
- **WHEN** 收到 ForceSleepReady
- **THEN** 冻结当前场景，切换到 house 场景，设置玩家位置到 BED 旁边

#### Scenario: 恢复体力
- **WHEN** 收到 ForceSleepReady
- **THEN** energy.restore(energy.max_energy * 0.5)（恢复 50% 最大体力）

#### Scenario: 推进天数
- **WHEN** 收到 ForceSleepReady
- **THEN** clock.day += 1, clock.time_slot = 0

#### Scenario: 恢复时钟
- **WHEN** 所有状态重置完成
- **THEN** 调用 clock.resume()

#### Scenario: 发送响应
- **WHEN** 所有状态重置完成
- **THEN** 发送 SceneChangeResp + ClockSync + EnergySync 给客户端

### Requirement: 客户端场景切换
客户端 SHALL 在收到响应后完成场景切换和 Iris 展开。

#### Scenario: 收到 SceneChangeResp
- **WHEN** 客户端收到 SceneChangeResp（house 场景）
- **THEN** 替换当前 TileMap 为 house 场景，更新玩家位置

#### Scenario: Iris 展开
- **WHEN** 场景替换完成
- **THEN** 启动 Iris 展开过渡（300ms）

#### Scenario: 恢复游戏
- **WHEN** Iris 展开完成（状态回到 IDLE）
- **THEN** 恢复游戏输入，显示新的时间和能量

### Requirement: 异常处理
强制睡觉流程 SHALL 处理网络异常情况。

#### Scenario: 客户端断线
- **WHEN** ForceSleepNotify 发送后客户端断线
- **THEN** 服务端仍完成场景切换和天数推进，客户端重连后获取最新状态

#### Scenario: 超时处理
- **WHEN** ForceSleepNotify 发送后 10 秒未收到 ForceSleepReady
- **THEN** 服务端强制完成切换流程
