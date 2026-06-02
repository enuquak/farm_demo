# farm_demo

农场模拟游戏 Demo - 包含客户端和服务器端实现

## 项目结构

```
farm_demo/
├── .claude/              # Claude 配置和技能
├── .gitignore            # Git 忽略规则
├── CMakeLists.txt        # 根构建配置
├── README.md             # 项目说明
├── bin/                  # 编译输出目录
├── config/               # 配置文件
│   ├── dbmgr.json        # 数据库管理器配置
│   ├── game_server.json  # 游戏服务器配置
│   ├── gate_server.json  # 网关服务器配置
│   └── mongo/            # MongoDB 索引配置
├── docs/                 # 文档
│   ├── pre_knowledge/    # 预知识文档
│   └── superpowers/      # 设计文档和计划
├── openspec/             # 规格文档
├── scripts/              # 源代码
│   ├── client/           # 客户端代码（Python）
│   ├── common/           # 共享代码（Proto 定义）
│   └── server/           # 服务器代码（C++）
│       ├── chat_server/  # 聊天服务器
│       ├── common/       # 服务器共享代码
│       ├── cross_server/ # 跨服服务器
│       ├── dbmgr_server/ # 数据库管理器
│       ├── friend_service/ # 好友服务
│       ├── game_server/  # 游戏服务器
│       ├── gate_server/  # 网关服务器
│       └── team_server/  # 组队服务器
├── tables/               # 表格数据
├── tests/                # 测试文件
├── tmp/                  # 临时文件
└── tools/                # 工具脚本
```

## 快速开始

### 1. 环境准备

- C++17 编译器（MSVC 2022）
- Python 3.8+
- CMake 3.14+
- MongoDB
- Redis
- etcd（可选）

### 2. 依赖安装

```bash
# 安装 Python 依赖
pip install -r requirements.txt

# 安装 C++ 依赖（Windows）
# libevent, protobuf, hiredis, mongoc 等
```

### 3. 构建项目

```bash
# 使用构建脚本
tools\build_all.bat

# 或手动构建
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 4. 配置

编辑 `config/` 目录下的 JSON 配置文件：

- `dbmgr.json` - 数据库连接配置
- `game_server.json` - 游戏服务器配置
- `gate_server.json` - 网关服务器配置

### 5. 启动服务

```bash
# 启动所有服务
tools\start_all.bat

# 或手动启动
bin\dbmgr_server.exe --config config\dbmgr.json
bin\game_server.exe --config config\game_server.json
bin\gate_server.exe --config config\gate_server.json
```

### 6. 停止服务

```bash
# 优雅停止
tools\stop_graceful.bat

# 强制停止
tools\stop_force.bat
```

## 服务器架构

### 核心组件

| 组件 | 端口 | 说明 |
|------|------|------|
| gate_server | 8080 | 网关服务器，处理客户端连接 |
| game_server | 9090 | 游戏服务器，处理游戏逻辑 |
| dbmgr_server | 5000 | 数据库管理器，处理数据存储 |
| chat_server | - | 聊天服务器 |
| friend_service | - | 好友服务 |
| cross_server | - | 跨服服务器 |
| team_server | - | 组队服务器 |

### 数据存储

- **MongoDB** - 持久化存储（玩家数据、账号数据）
- **Redis** - 缓存层（热数据、会话数据）
- **etcd** - 服务发现和配置中心（可选）

## 开发指南

### 运行测试

```bash
# Python 测试
pytest tests/

# C++ 测试
cd build
ctest --config Release
```

### 代码生成

```bash
# 生成共享常量
python tools/generate_constants.py

# 生成 Proto 代码
protoc --proto_path=scripts/common/proto --cpp_out=scripts/common/proto/generated scripts/common/proto/*.proto
```

### 工具

- `tools/config_editor.py` - 配置编辑器
- `tools/quest_editor/` - 任务编辑器
- `tools/server_console/` - 服务器控制台
- `tools/table_export/` - 表格导出工具

## 业务功能

### 已完成功能

- [x] 登录功能
- [x] 场景功能
- [x] 玩家实体
- [x] 玩家移动
- [x] 场景元素功能
- [x] 背包，物品
- [x] 交互
- [x] 体力
- [x] 场景切换
- [x] NPC 对话
- [x] 任务编辑器
- [x] 配置编辑器 and 导表
- [x] 怪兽
- [x] 战斗
- [x] 聊天
- [x] 组队功能
- [x] 跨服组队功能

### 服务器功能

- [x] 数据库使用 MongoDB
- [x] 进程向 etcd 注册
- [x] 服务器配置管理
- [x] 服务器管理器
- [x] 可视化 GM 平台
- [x] 接入 Redis
- [x] 微服务架构
- [x] 玩家数据存写方案
- [x] 角色 player_id 全服唯一标识
- [x] 单测系统

## 边界处理

- **DBMgr 故障恢复** - 进程启动后占用文件符，通过 etcd 检测故障，自动切换到下一个 dbmgr，使用一致性哈希

## 文档

- [设计文档](docs/superpowers/specs/) - 系统设计文档
- [实现计划](docs/superpowers/plans/) - 实现计划
- [规格文档](openspec/specs/) - 功能规格

## 许可证

私有项目
