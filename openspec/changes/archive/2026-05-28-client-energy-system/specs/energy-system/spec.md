## Purpose

能量系统：在服务端管理玩家能量数据（当前值/最大值），支持消耗与恢复，每次物品交互消耗对应能量，能量不足时阻止交互并通知客户端显示弹窗。

## Requirements

### Requirement: 能量数据模型
Energy 类 SHALL 管理当前能量值和最大能量值。

#### Scenario: 初始化能量
- **WHEN** 新玩家首次进入游戏
- **THEN** 创建 Energy 实例，current=100, max_energy=100

#### Scenario: 消耗能量
- **WHEN** 调用 `energy.consume(4)` 且 current=100
- **THEN** current 更新为 96，返回 True

#### Scenario: 能量不足
- **WHEN** 调用 `energy.consume(4)` 且 current=2
- **THEN** current 保持 2 不变，返回 False

#### Scenario: 恢复能量
- **WHEN** 调用 `energy.restore(15)` 且 current=80, max_energy=100
- **THEN** current 更新为 95

#### Scenario: 恢复不超过上限
- **WHEN** 调用 `energy.restore(15)` 且 current=95, max_energy=100
- **THEN** current 更新为 100（不超过 max）

### Requirement: 交互能量消耗
物品使用前 SHALL 检查并扣除能量。

#### Scenario: 能量充足时使用物品
- **WHEN** 斧头砍石头（energy_cost=4），current=50
- **THEN** 扣除 4 点能量，current=46，执行效果

#### Scenario: 能量不足时阻止
- **WHEN** 斧头砍石头（energy_cost=4），current=3
- **THEN** 返回 ENERGY_EXHAUSTED 错误码，不执行效果

#### Scenario: 食物恢复能量
- **WHEN** 吃面包（energy_restore=15），current=70
- **THEN** current 更新为 85，消耗面包

#### Scenario: 免费交互
- **WHEN** 使用的效果无 energy_cost 字段
- **THEN** 不消耗能量，直接执行效果

### Requirement: 精疲力尽通知
服务器 SHALL 在能量不足时通知客户端显示弹窗。

#### Scenario: 返回精疲力尽错误码
- **WHEN** 能量不足导致交互失败
- **THEN** ItemUseResp code=ENERGY_EXHAUSTED

#### Scenario: 客户端显示弹窗
- **WHEN** 客户端收到 code=ENERGY_EXHAUSTED 的 ItemUseResp
- **THEN** 显示"精疲力尽！能量不足，无法执行此操作"弹窗

### Requirement: 能量条 UI
客户端 SHALL 在右下角显示能量条。

#### Scenario: 能量条布局
- **WHEN** 游戏运行中
- **THEN** 右下角显示 40×120 像素的竖条能量条

#### Scenario: 填充方向
- **WHEN** current=75, max=100
- **THEN** 能量条从下往上填充 75%，显示 "75/100" 数字

#### Scenario: 颜色变化
- **WHEN** current=80 (ratio=0.8)
- **THEN** 能量条颜色为绿色 (#4caf50)

#### Scenario: 中等能量
- **WHEN** current=40 (ratio=0.4)
- **THEN** 能量条颜色为橙黄色 (#ff9800)

#### Scenario: 低能量
- **WHEN** current=20 (ratio=0.2)
- **THEN** 能量条颜色为红色 (#f44336)

### Requirement: 精疲力尽弹窗
客户端 SHALL 在能量不足时显示模态弹窗。

#### Scenario: 弹窗显示
- **WHEN** 收到 ENERGY_EXHAUSTED 错误码
- **THEN** 显示居中弹窗，包含文字"精疲力尽！"和"确定"按钮

#### Scenario: 弹窗暂停游戏
- **WHEN** 弹窗显示中
- **THEN** 游戏输入被屏蔽（移动、交互均不可用）

#### Scenario: 关闭弹窗
- **WHEN** 玩家点击"确定"或按 Enter/Escape
- **THEN** 关闭弹窗，恢复游戏输入

### Requirement: 能量同步
服务器 SHALL 在登录和能量变更时同步数据给客户端。

#### Scenario: 登录时同步
- **WHEN** 玩家登录成功
- **THEN** 服务器发送 EnergySync {current, max}

#### Scenario: 变更时同步
- **WHEN** 能量因交互发生变化
- **THEN** ItemUseResp 中包含 EnergySync 字段

### Requirement: 能量持久化
能量数据 SHALL 纳入 DBMgr 持久化。

#### Scenario: 保存
- **WHEN** 玩家下线
- **THEN** energy.serialize() 数据写入 DBMgr

#### Scenario: 加载
- **WHEN** 玩家登录，DBMgr 返回 energy 字段
- **THEN** 调用 Energy.deserialize(data) 恢复

#### Scenario: 旧存档兼容
- **WHEN** 存档中无 energy 字段
- **THEN** 默认 current=100, max=100
