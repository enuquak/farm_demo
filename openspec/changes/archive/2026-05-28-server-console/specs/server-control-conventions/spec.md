## Purpose

AI 服务器控制规范，约束 AI 智能体对 farm_demo 服务器进程的操作行为，确保操作一致性、安全性和可追溯性。

## Requirements

### Requirement: 重启操作规范

AI 智能体 SHALL 使用重启（Restart）操作管理服务器，而非单独启停。

#### Scenario: AI 执行服务器重启
- **WHEN** AI 需要重启服务器（如测试、配置变更后）
- **THEN** AI SHALL 执行以下步骤:
  1. 确认当前无其他服务器操作正在进行
  2. 调用重启流程（先停后起）
  3. 等待重启完成
  4. 验证所有服务正常运行

#### Scenario: 禁止单独启停
- **WHEN** AI 需要操作服务器
- **THEN** AI SHALL NOT 单独执行 start 或 stop 操作
- **AND** AI SHALL 使用 restart_all() 完成先停后起的完整流程

### Requirement: 停服优先级

AI 智能体 SHALL 优先使用优雅停服，仅在超时时使用强制停服。

#### Scenario: 优雅停服
- **WHEN** AI 执行停服操作
- **THEN** AI SHALL 先发送 MSG_ID_SHUTDOWN (5001) 到 gate_server:8080
- **AND** 等待进程退出（最多 timeout_ms）
- **AND** 超时后才 fallback 到 taskkill /F

#### Scenario: 强制停服作为后备
- **WHEN** 优雅停服超时或 TCP 连接失败
- **THEN** AI SHALL 使用 taskkill /PID <pid> /F 强制终止
- **AND** 清理对应的 PID 文件

### Requirement: 日志读取规范

AI 智能体 SHALL 通过指定路径和格式读取服务器日志。

#### Scenario: 日志文件路径
- **WHEN** AI 需要读取服务器日志
- **THEN** 日志文件路径 SHALL 为:
  - `runtimeData/logs/server/dbmgr.log`
  - `runtimeData/logs/server/game_server.log`
  - `runtimeData/logs/server/gate_server.log`

#### Scenario: 日志格式解析
- **WHEN** AI 解析日志行
- **THEN** 日志格式 SHALL 为: `[YYYY-MM-DD HH:MM:SS.mmm][level][name:pid] [Module]message`
- **AND** AI SHALL 能按 level（info/warn/error）过滤
- **AND** AI SHALL 能按 Module（Main/Gate/Game/DBMgr 等）过滤

#### Scenario: 错误日志检测
- **WHEN** AI 操作服务器后
- **THEN** AI SHALL 检查各服务日志中是否有 error 级别输出
- **AND** 发现错误 SHALL 报告给用户

### Requirement: PID 文件管理规范

AI 智能体 SHALL 通过 PID 文件判断服务状态。

#### Scenario: PID 文件路径
- **WHEN** AI 需要检查服务状态
- **THEN** PID 文件路径 SHALL 为:
  - `runtimeData/dbmgr.pid`
  - `runtimeData/game_server.pid`
  - `runtimeData/gate_server.pid`

#### Scenario: 进程存活检测
- **WHEN** AI 读取 PID 文件
- **THEN** AI SHALL 通过 PID 判断进程是否存活
- **AND** PID 文件不存在 SHALL 视为服务未运行

### Requirement: 启动顺序约束

AI 智能体 SHALL 按照依赖顺序启动服务。

#### Scenario: 正确的启动顺序
- **WHEN** AI 启动服务
- **THEN** 启动顺序 SHALL 为:
  1. dbmgr（无依赖）
  2. game_server（依赖 dbmgr）
  3. gate_server（依赖 game_server）
- **AND** 每步间隔 SHALL 至少 2 秒

#### Scenario: 停服顺序
- **WHEN** AI 停止服务
- **THEN** 停止顺序 SHALL 为启动顺序的逆序:
  1. gate_server
  2. game_server
  3. dbmgr

### Requirement: 操作验证

AI 智能体 SHALL 在操作后验证结果。

#### Scenario: 启动验证
- **WHEN** AI 完成启动操作
- **THEN** AI SHALL 验证:
  1. PID 文件已写入
  2. 进程存活
  3. 日志中无启动错误

#### Scenario: 停服验证
- **WHEN** AI 完成停服操作
- **THEN** AI SHALL 验证:
  1. 所有进程已退出
  2. PID 文件已清理（优雅停服时由服务自行清理）
