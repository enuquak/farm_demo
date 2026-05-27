## Purpose

客户端登录界面模块，基于 PyGame 实现，提供账号登录、自动角色管理和游戏进入功能。作为客户端启动后的第一个交互界面，支持完整的登录流程和错误处理。

## Requirements

### Requirement: 登录界面显示
系统 SHALL 在启动时显示登录界面，包含账号输入框和进入按钮。

#### Scenario: 启动显示登录界面
- **WHEN** 客户端启动
- **THEN** 显示登录界面，包含标题"Farm Demo"、账号 ID 输入框、"Enter Game" 按钮

#### Scenario: 输入账号 ID
- **WHEN** 用户在输入框中输入 account_id
- **THEN** 输入框实时显示输入内容，支持退格删除

#### Scenario: 回车触发登录
- **WHEN** 用户在输入框中按下回车键
- **THEN** 系统开始连接服务器并执行登录流程

#### Scenario: 点击按钮触发登录
- **WHEN** 用户点击"Enter Game"按钮
- **THEN** 系统开始连接服务器并执行登录流程

### Requirement: 空输入校验
系统 SHALL 校验 account_id 不为空。

#### Scenario: 空账号尝试登录
- **WHEN** 用户未输入任何内容就按回车或点击按钮
- **THEN** 显示错误提示"请输入账号 ID"，不发起连接

### Requirement: 自动角色管理
系统 SHALL 在登录后自动查询角色列表，无角色则自动创建，有角色则直接进入游戏。

#### Scenario: 新账号自动创建角色
- **WHEN** 登录成功后查询角色列表为空
- **THEN** 系统自动发送 CreateRoleReq（role_name = account_id, server_id = 1），创建成功后自动进入游戏

#### Scenario: 已有角色直接进入
- **WHEN** 登录成功后查询角色列表非空
- **THEN** 系统使用第一个角色的 player_id 发送 EnterGameReq，直接进入游戏

#### Scenario: 进入游戏成功
- **WHEN** EnterGameResp 返回 code=0
- **THEN** 系统切换到 PLAYING 状态，传入 PlayerData（role_name, pos_x, pos_y, scene_id）

### Requirement: 连接中状态反馈
系统 SHALL 在登录流程执行期间显示进度反馈。

#### Scenario: 连接中显示加载状态
- **WHEN** 用户触发登录，正在执行网络流程
- **THEN** 显示"连接中..."或类似的加载提示，禁用输入框和按钮

### Requirement: 错误处理
系统 SHALL 在登录流程中任何步骤失败时显示错误信息并允许重试。

#### Scenario: 连接服务器失败
- **WHEN** 无法连接到 Gate Server（超时或拒绝）
- **THEN** 显示错误信息"连接服务器失败: {原因}"，返回登录界面允许重试

#### Scenario: 登录请求失败
- **WHEN** LoginResp 返回 code != 0
- **THEN** 显示错误信息"登录失败: {msg}"，返回登录界面允许重试

#### Scenario: 创建角色失败
- **WHEN** CreateRoleResp 返回 code != 0
- **THEN** 显示错误信息"创建角色失败: {msg}"，返回登录界面允许重试

#### Scenario: 进入游戏失败
- **WHEN** EnterGameResp 返回 code != 0
- **THEN** 显示错误信息"进入游戏失败: {msg}"，返回登录界面允许重试

#### Scenario: 网络流程超时
- **WHEN** 任何网络步骤超过 10 秒未响应
- **THEN** 显示错误信息"操作超时"，返回登录界面允许重试
