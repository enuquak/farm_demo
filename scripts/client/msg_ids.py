"""
消息 ID 常量定义
与 scripts/common/proto/msg_ids.h 保持一致
"""

# 基础消息 (0-999)
MSG_ID_HEARTBEAT = 1
MSG_ID_HEARTBEAT_RESP = 2
MSG_ID_LOGIN_REQ = 3
MSG_ID_LOGIN_RESP = 4

# 账号相关 (1000-1999)
MSG_ID_QUERY_ROLES_REQ = 1001
MSG_ID_QUERY_ROLES_RESP = 1002
MSG_ID_CREATE_ROLE_REQ = 1003
MSG_ID_CREATE_ROLE_RESP = 1004

# 进入游戏 (1005-1006, AccountMsg 通道)
MSG_ID_ENTER_GAME_REQ = 1005
MSG_ID_ENTER_GAME_RESP = 1006

# 场景数据 (2000-2099)
MSG_ID_MAP_DATA_NOTIFY = 2001

# 玩家移动 (2100-2199)
MSG_ID_POSITION_UPDATE = 2101       # Client -> Game: 位置更新
MSG_ID_POSITION_CORRECT = 2102      # Game -> Client: 位置纠正

# 物品交互 (3000-3099)
MSG_ID_ITEM_USE_REQ = 3001          # Client -> Game: 物品使用请求
MSG_ID_ITEM_USE_RESP = 3002         # Game -> Client: 物品使用响应
MSG_ID_DROP_ITEM_SYNC = 3003        # Game -> Client: 掉落物同步

# 背包同步 (3100-3199)
MSG_ID_INVENTORY_SYNC = 3100        # Game -> Client: 背包数据同步
MSG_ID_ACTIVE_SLOT_CHANGE = 3101    # Client -> Game: 快捷栏选中格切换

# 场景切换 (2200-2299)
MSG_ID_SCENE_CHANGE_REQ = 2201      # Client -> Game: 场景切换请求
MSG_ID_SCENE_CHANGE_RESP = 2202     # Game -> Client: 场景切换响应

# 游戏时钟 (2300-2399)
MSG_ID_CLOCK_SYNC = 2301            # Game -> Client: 时钟同步
MSG_ID_FORCE_SLEEP_NOTIFY = 2302    # Game -> Client: 强制睡觉通知
MSG_ID_FORCE_SLEEP_READY = 2303     # Client -> Game: 强制睡觉就绪
