## Purpose

Python GUI 服务器控制台工具，基于 CustomTkinter 构建，提供服务器进程管理（重启、强制停服）和实时日志查看功能。作为现有 batch 脚本的替代操作入口，支持 dbmgr、game_server、gate_server 三个进程及未来多个 game_server 实例。

## Requirements

### Requirement: 一键重启所有服务

系统 SHALL 提供 Restart All 按钮，执行先停后起的完整重启流程。

#### Scenario: 正常重启流程
- **WHEN** 用户点击 Restart All 按钮
- **THEN** 系统 SHALL 按以下顺序执行:
  1. 逆序停止：gate_server → game_server → dbmgr
  2. 等待所有进程退出
  3. 正序启动：dbmgr → game_server → gate_server
  4. 每步间隔 2 秒
  5. 更新 UI 状态指示

#### Scenario: 重启过程中有服务未运行
- **WHEN** Restart All 执行时某服务未运行
- **THEN** 系统 SHALL 跳过该服务的停止步骤
- **AND** 继续执行后续启动步骤

#### Scenario: 重启按钮状态锁定
- **WHEN** 重启流程执行中
- **THEN** Restart All 按钮 SHALL 禁用
- **AND** Force Stop 按钮 SHALL 保持可用

### Requirement: 强制停服

系统 SHALL 提供 Force Stop 按钮，强制终止所有运行中的服务进程。

#### Scenario: 强制停服流程
- **WHEN** 用户点击 Force Stop 按钮
- **THEN** 系统 SHALL:
  1. 读取各服务的 PID 文件（`runtimeData/{name}.pid`）
  2. 使用 `taskkill /PID <pid> /F` 强制终止每个进程
  3. 清理 PID 文件
  4. 更新 UI 状态指示

#### Scenario: PID 文件不存在
- **WHEN** 强制停服时某服务的 PID 文件不存在
- **THEN** 系统 SHALL 跳过该服务并继续处理其他服务

### Requirement: 服务状态显示

系统 SHALL 实时显示各服务的运行状态。

#### Scenario: 状态检测
- **WHEN** 系统运行时
- **THEN** 系统 SHALL 每 2 秒检测一次各服务的 PID 文件
- **AND** 通过 PID 判断进程是否存活
- **AND** 在页签标题上显示状态指示（● 运行中 / ○ 已停止）

#### Scenario: 状态栏计数
- **WHEN** 服务状态变化
- **THEN** 工具栏状态文本 SHALL 显示 "Status: X/3"（X 为运行中的服务数）

### Requirement: 日志页签展示

系统 SHALL 为每个服务提供独立的日志页签。

#### Scenario: 默认页签
- **WHEN** 控制台启动
- **THEN** 系统 SHALL 创建以下页签:
  - dbmgr
  - game_server
  - gate_server

#### Scenario: 切换页签
- **WHEN** 用户点击某服务的页签
- **THEN** 系统 SHALL 清空当前日志显示区域
- **AND** 从该服务的 LogReader 缓存中回放所有日志行
- **AND** 自动滚动到底部

#### Scenario: 自动滚动
- **WHEN** 日志显示区域已滚动到底部
- **THEN** 新日志行 SHALL 自动追加并滚动
- **WHEN** 用户向上滚动查看历史日志
- **THEN** 自动滚动 SHALL 暂停
- **WHEN** 用户再次滚动到底部
- **THEN** 自动滚动 SHALL 恢复

### Requirement: 日志实时读取

系统 SHALL 通过后台线程持续读取各服务的日志文件。

#### Scenario: 日志文件尾部读取
- **WHEN** 服务运行中
- **THEN** LogReader SHALL 以后台线程持续读取 `runtimeData/logs/server/{name}.log`
- **AND** 新增行 SHALL 写入 `deque(maxlen=10000)` 缓存
- **AND** 通知 GUI 更新（如果当前正在查看该服务的页签）

#### Scenario: 日志文件不存在
- **WHEN** 服务的日志文件尚不存在
- **THEN** LogReader SHALL 每 1 秒重试检测文件
- **AND** 文件创建后自动开始读取

#### Scenario: 日志文件轮转
- **WHEN** spdlog 执行日志文件轮转（文件名变化）
- **THEN** LogReader SHALL 检测到文件变化并重新打开新文件
- **AND** 不丢失已缓存的日志行

### Requirement: 启动命令配置

系统 SHALL 使用与现有 batch 脚本相同的命令格式启动服务。

#### Scenario: 服务启动命令
- **WHEN** 启动单个服务
- **THEN** 命令格式 SHALL 为: `bin\{service_name}.exe --config config\{service_name}.json`
- **AND** 工作目录 SHALL 为项目根目录
- **AND** 子进程的 stdout/stderr SHALL 不重定向（由服务自身管理日志）

#### Scenario: 启动后验证
- **WHEN** 服务启动后
- **THEN** 系统 SHALL 等待最多 5 秒检测 PID 文件写入
- **AND** PID 文件存在则认为启动成功
- **AND** 超时则认为启动失败并中止后续服务启动

### Requirement: 多 game_server 支持（预留）

系统 SHALL 预留多个 game_server 实例的管理能力。

#### Scenario: 动态页签
- **WHEN** 配置中存在多个 game_server 实例
- **THEN** 系统 SHALL 为每个实例创建独立页签
- **AND** 页签名称 SHALL 包含实例标识（如 game_server_1, game_server_2）

#### Scenario: 初期限制
- **WHEN** 当前版本
- **THEN** 系统 SHALL 默认只管理 1 个 game_server 实例
- **AND** 多实例支持作为后续迭代目标
