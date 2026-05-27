## Context

当前 `scripts/client/` 下只有网络测试代码：`GateConnection`（TCP 长连接 + 心跳）、`MessageHandler`（消息分发）、`HeartbeatManager`。这些模块已经实现了与 Gate Server 的可靠通信，但没有任何图形界面。

服务端已完成：Gate Server（消息路由）→ Game Server（玩家逻辑）→ DBMgr（数据持久化）的完整链路。协议层支持 LoginReq(3)、AccountMsg(1001/1003/1005) 等消息，客户端可以直接串联使用。

项目计划使用 Python + PyGame 作为客户端技术栈。

## Goals / Non-Goals

**Goals:**
- 实现完整的登录 → 进入游戏 → 场景渲染链路
- 星露谷风格的 2D 像素风渲染（16×16 瓦片，4x 缩放）
- 玩家可在场景中用 WASD/方向键移动，带 4 方向行走动画
- 登录流程自动化：输入 account_id 后自动完成所有网络握手
- 复用现有 `GateConnection` 网络层，不重写

**Non-Goals:**
- 不做多人同步（场景中只有自己的角色）
- 不做农场玩法（种田、背包、NPC）— 后续 change 实现
- 不做服务端改动
- 不做音效/音乐
- 不做存档/读档（服务端已有数据持久化）

## Decisions

### D1: 渲染框架 — PyGame Surface 渲染，不用 PyOpenGL

**选择**: PyGame 原生 Surface 渲染

**理由**: PyGame 的 Surface.blit() 对 2D 瓦片游戏完全够用。16×16 瓦片在 4x 缩放下，可见区域约 16×12 瓦片（~192 个 blit/帧），远在性能舒适区内。PyOpenGL 增加了复杂度但无实际收益。

**替代方案**: PyGame + PyOpenGL — 适合需要 shader 或旋转/缩放特效的场景，当前不需要。

### D2: 地图系统 — pytmx + pyscroll

**选择**: 使用 Tiled Map Editor 编辑地图，pytmx 加载，pyscroll 渲染

**理由**:
- pytmx 是成熟的 TMX 文件加载库，支持多层、对象层、瓦片属性
- pyscroll 自动处理视口裁剪（只渲染可见瓦片）、缓冲渲染、精灵与图层的遮挡排序
- Tiled 编辑器免费且是行业标准，方便后续调整地图

**替代方案**: 自定义 2D 数组地图 — 需要自己实现裁剪、相机、图层管理，工作量大且容易出 bug。

### D3: 瓦片规格 — 16×16 原生，4x 整数缩放

**选择**: 16×16 像素原生瓦片，渲染时 4x 缩放到 64×64

**理由**: 星露谷物语使用 16×16 瓦片。4x 缩放后窗口分辨率 1024×768（16×12 瓦片可见），是舒适的农场游戏视野。整数缩放保持像素风清晰，避免插值模糊。

**替代方案**: 32×32 瓦片 2x 缩放 — 更细腻但制作成本更高，且农场游戏不需要那么多细节。

### D4: 状态机架构 — LOGIN → CONNECTING → PLAYING

**选择**: 三个状态，Game 类持有当前状态并委托 update/draw

**理由**:
- LOGIN 状态：纯 UI，处理键盘输入 account_id
- CONNECTING 状态：自动执行网络流程（登录→查角色→创角色/进游戏），UI 显示进度
- PLAYING 状态：渲染场景 + 处理移动输入
- 状态之间通过返回值传递数据（如 account_id、player_data）

**替代方案**: 单一状态 + 标志位 — 容易变成面条代码，难以维护。

### D5: 网络集成 — 封装 GameNetwork 类串联协议流程

**选择**: 新建 `network.py`，封装 `GameNetwork` 类，在 CONNECTING 状态中同步执行登录流程

**理由**: 现有 `GateConnection` 是异步的（网络线程 + 消息队列）。需要一个上层封装来串联多步协议流程：

```
GameNetwork.login_flow(account_id) →
  1. connect()
  2. send LoginReq → wait LoginResp
  3. send QueryRolesReq → wait QueryRolesResp
  4. if empty: send CreateRoleReq → wait CreateRoleResp
  5. send EnterGameReq → wait EnterGameResp
  6. return PlayerData
```

每一步带超时，失败时返回错误信息供 UI 显示。

**替代方案**: 在 Game 主循环中逐步轮询 — 状态分散在多帧中，代码难以跟踪。

### D6: 美术资源 — CC0 免费像素风资源包

**选择**: 使用 Kenney 等 CC0 授权的免费像素风资源

**理由**: 避免版权问题，快速获得可用的美术资源。后续可以用自定义资源替换。

**资源清单**:
- 瓦片集：草地、泥土地、石径、水、围栏、树木、建筑
- 角色精灵表：16×16 或 32×32，4 方向×4 帧行走动画
- 字体：像素风字体（如 Press Start 2P）

**替代方案**: 程序化生成瓦片 — 可作为 fallback，但视觉效果有限。

### D7: 文件结构

```
scripts/client/
├── main.py                    # 入口
├── settings.py                # 常量配置
├── requirements.txt           # 依赖
├── game.py                    # Game 类 + 状态机
├── network.py                 # GameNetwork 协议流程封装
├── camera.py                  # 摄像机
├── tilemap.py                 # 地图加载与渲染
├── spritesheet.py             # 精灵表工具
├── connection.py              # (已有) GateConnection
├── heartbeat.py               # (已有) HeartbeatManager
├── message_handler.py         # (已有) MessageHandler
├── log_init.py                # (已有) 日志初始化
├── log_modules.py             # (已有) 日志模块
├── entities/
│   ├── __init__.py
│   └── player.py              # 玩家精灵
├── ui/
│   ├── __init__.py
│   ├── login_screen.py        # 登录界面
│   └── hud.py                 # 游戏内 HUD
├── states/
│   ├── __init__.py
│   ├── base_state.py          # 状态基类
│   ├── login_state.py         # 登录状态
│   └── play_state.py          # 游戏状态
└── assets/
    ├── tilesets/               # 瓦片图集 PNG
    ├── maps/                   # Tiled .tmx 地图
    ├── sprites/                # 角色精灵表
    └── fonts/                  # 字体文件
```

## Risks / Trade-offs

**[R1] 美术资源获取** → 使用 CC0 资源包作为起点。如果找不到合适的资源，用程序化生成的彩色方块作为 fallback，确保功能链路可跑通。

**[R2] pytmx/pyscroll 兼容性** → 这两个库是成熟项目（10+ 年历史），但需注意 Python 3.12 兼容性。如果出现问题，可以降级到自定义瓦片渲染（工作量增加约 2 天）。

**[R3] 网络阻塞** → CONNECTING 状态中的登录流程是同步阻塞的（带超时）。这意味着连接期间 UI 不响应。可接受，因为整个流程通常在 1-2 秒内完成。后续可以改为异步。

**[R4] 精灵表格式** → 需要与所选资源包的精灵表布局匹配。如果布局不标准，需要在 `spritesheet.py` 中做适配。
