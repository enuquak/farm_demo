## Purpose

背包界面：基于 PyGame 实现底部快捷栏和完整背包面板，像素风格渲染，显示物品图标和数量，支持键盘切换选中格和面板开关。

## Requirements

### Requirement: 快捷栏显示
系统 SHALL 在屏幕底部固定显示 10 格快捷栏。

#### Scenario: 快捷栏布局
- **WHEN** 游戏运行中
- **THEN** 屏幕底部居中显示 10 格横向排列的物品栏，每格 TILE_SIZE×TILE_SIZE

#### Scenario: 显示物品图标
- **WHEN** slot[0]=斧头×1
- **THEN** 快捷栏第 1 格显示斧头图标和数量 "1"

#### Scenario: 空 slot 显示
- **WHEN** slot[4]=None
- **THEN** 快捷栏第 5 格显示空格子边框

#### Scenario: 选中高亮
- **WHEN** activeSlot=2
- **THEN** 快捷栏第 3 格显示高亮边框（白色或黄色）

### Requirement: 物品图标渲染
系统 SHALL 为每种物品预生成 8×8 像素图标 Surface。

#### Scenario: 图标生成
- **WHEN** 系统初始化
- **THEN** 为每种物品定义中的 icon palette 数据生成 8×8 PyGame Surface，拉伸到 32×32

#### Scenario: 图标缓存
- **WHEN** 多个 slot 都有同种物品
- **THEN** 共享同一个图标 Surface 实例

### Requirement: 数字键切换
玩家 SHALL 能通过数字键 1~0 切换快捷栏选中格。

#### Scenario: 按 1 键
- **WHEN** 玩家按下数字键 1
- **THEN** activeSlot 切换为 0，发送 ActiveSlotChange 消息给服务器

#### Scenario: 按 0 键
- **WHEN** 玩家按下数字键 0
- **THEN** activeSlot 切换为 9，发送 ActiveSlotChange 消息给服务器

### Requirement: 背包面板开关
玩家 SHALL 能通过 E 键打开/关闭完整背包面板。

#### Scenario: 按 E 键打开
- **WHEN** 背包面板关闭，玩家按下 E 键
- **THEN** 打开背包面板，覆盖在游戏画面上，暂停角色移动

#### Scenario: 按 E 键关闭
- **WHEN** 背包面板打开，玩家按下 E 键
- **THEN** 关闭背包面板，恢复角色移动

#### Scenario: 面板打开时屏蔽游戏输入
- **WHEN** 背包面板打开
- **THEN** WASD 移动、空格交互、鼠标点击交互 SHALL 被屏蔽

### Requirement: 背包面板布局
背包面板 SHALL 显示完整的 30 格网格布局。

#### Scenario: 面板布局
- **WHEN** 背包面板打开
- **THEN** 居中显示 10×3 网格：第 1 行为快捷栏（10 格），第 2~3 行为扩展背包（20 格）

#### Scenario: 面板标题
- **WHEN** 背包面板打开
- **THEN** 面板顶部显示 "背包" 标题

#### Scenario: 面板关闭按钮
- **WHEN** 背包面板打开
- **THEN** 面板右上角显示 [×] 关闭按钮，点击可关闭面板

### Requirement: 背包数据同步
客户端 SHALL 根据服务器的 InventorySync 消息更新 UI。

#### Scenario: 收到 InventorySync
- **WHEN** 客户端收到 InventorySync 消息
- **THEN** 更新本地背包数据，刷新快捷栏和背包面板的显示

#### Scenario: 仅更新变化的 slot
- **WHEN** InventorySync 仅包含部分 slot 变更
- **THEN** 仅更新对应 slot 的显示，不全量刷新（优化性能）
