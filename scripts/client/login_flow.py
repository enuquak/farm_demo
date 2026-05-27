"""
登录流程管理模块
处理从连接到进入游戏的完整登录流程
"""
import sys
import os
import time
import threading
from enum import Enum
from typing import Optional, Callable, Dict, Any

# 添加 protobuf 生成目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'common', 'proto', 'generated'))

import base_pb2
import account_pb2
import player_pb2

from .connection import GateConnection, ConnectionState
from .msg_ids import (
    MSG_ID_LOGIN_REQ, MSG_ID_LOGIN_RESP,
    MSG_ID_QUERY_ROLES_REQ, MSG_ID_QUERY_ROLES_RESP,
    MSG_ID_CREATE_ROLE_REQ, MSG_ID_CREATE_ROLE_RESP,
    MSG_ID_ENTER_GAME_REQ, MSG_ID_ENTER_GAME_RESP
)


class LoginState(Enum):
    """登录状态枚举"""
    IDLE = 0                # 空闲状态
    CONNECTING = 1          # 连接服务器中
    LOGGING_IN = 2          # 登录中
    QUERYING_ROLES = 3      # 查询角色列表中
    CREATING_ROLE = 4       # 创建角色中
    ENTERING_GAME = 5       # 进入游戏中
    SUCCESS = 6             # 登录成功
    ERROR = 7               # 错误状态


class LoginFlowManager:
    """
    登录流程管理器
    管理从连接到进入游戏的完整流程
    """

    # 超时时间（秒）
    TIMEOUT_SECONDS = 10.0

    def __init__(self, host: str = '127.0.0.1', port: int = 8888):
        """
        初始化登录流程管理器

        Args:
            host: Gate 服务器地址
            port: Gate 服务器端口
        """
        self.host = host
        self.port = port

        # 连接对象
        self._connection: Optional[GateConnection] = None

        # 状态
        self._state = LoginState.IDLE
        self._state_lock = threading.Lock()

        # 账号 ID
        self._account_id: str = ""

        # 角色信息
        self._roles: list = []
        self._current_player_id: int = 0
        self._current_server_id: int = 1  # 默认服务器 ID

        # 超时检测
        self._step_start_time: float = 0
        self._timeout_timer: Optional[threading.Timer] = None

        # 回调函数
        self._on_state_change: Optional[Callable[[LoginState, str], None]] = None
        self._on_success: Optional[Callable[[Dict[str, Any]], None]] = None
        self._on_error: Optional[Callable[[str], None]] = None

        # 消息处理器映射
        self._response_handlers: Dict[int, Callable[[bytes], None]] = {
            MSG_ID_LOGIN_RESP: self._handle_login_resp,
            MSG_ID_QUERY_ROLES_RESP: self._handle_query_roles_resp,
            MSG_ID_CREATE_ROLE_RESP: self._handle_create_role_resp,
            MSG_ID_ENTER_GAME_RESP: self._handle_enter_game_resp,
        }

    @property
    def state(self) -> LoginState:
        """获取当前登录状态"""
        with self._state_lock:
            return self._state

    def set_on_state_change(self, callback: Callable[[LoginState, str], None]):
        """设置状态变化回调"""
        self._on_state_change = callback

    def set_on_success(self, callback: Callable[[Dict[str, Any]], None]):
        """设置登录成功回调"""
        self._on_success = callback

    def set_on_error(self, callback: Callable[[str], None]):
        """设置错误回调"""
        self._on_error = callback

    def start_login(self, account_id: str):
        """
        开始登录流程

        Args:
            account_id: 账号 ID
        """
        if not account_id or not account_id.strip():
            self._set_state(LoginState.ERROR, "请输入账号 ID")
            return

        self._account_id = account_id.strip()
        self._set_state(LoginState.CONNECTING, "连接中...")

        # 在新线程中执行连接
        connect_thread = threading.Thread(
            target=self._connect_to_server,
            name="LoginConnectThread",
            daemon=True
        )
        connect_thread.start()

    def cancel_login(self):
        """取消登录流程"""
        self._cancel_timeout_timer()
        if self._connection:
            self._connection.disconnect()
            self._connection = None
        self._set_state(LoginState.IDLE, "")

    def process_messages(self):
        """处理接收到的消息（应在主线程调用）"""
        if not self._connection:
            return

        messages = self._connection.recv_all_messages()
        for msg_id, payload in messages:
            handler = self._response_handlers.get(msg_id)
            if handler:
                handler(payload)

    def _connect_to_server(self):
        """连接到服务器（在后台线程执行）"""
        try:
            self._connection = GateConnection(self.host, self.port, timeout=5.0)
            self._connection.set_on_disconnect(self._on_disconnect)

            # 尝试连接
            self._connection.connect()

            # 连接成功，开始登录
            self._set_state(LoginState.LOGGING_IN, "登录中...")
            self._send_login_request()

        except TimeoutError:
            self._set_state(LoginState.ERROR, "连接服务器失败: 连接超时")
        except ConnectionRefusedError:
            self._set_state(LoginState.ERROR, "连接服务器失败: 连接被拒绝")
        except Exception as e:
            self._set_state(LoginState.ERROR, f"连接服务器失败: {str(e)}")

    def _send_login_request(self):
        """发送登录请求"""
        self._start_timeout_timer()

        # 创建 AccountMsg 包装登录请求
        login_req = base_pb2.LoginReq()
        login_req.token = self._account_id
        login_payload = login_req.SerializeToString()

        account_msg = base_pb2.AccountMsg()
        account_msg.account_id = self._account_id
        account_msg.msg_id = MSG_ID_LOGIN_REQ
        account_msg.payload = login_payload
        payload = account_msg.SerializeToString()

        self._connection.send_message(MSG_ID_LOGIN_REQ, payload)

    def _handle_login_resp(self, payload: bytes):
        """处理登录响应"""
        self._cancel_timeout_timer()

        login_resp = base_pb2.LoginResp()
        login_resp.ParseFromString(payload)

        if login_resp.code != 0:
            self._set_state(LoginState.ERROR, f"登录失败: {login_resp.msg}")
            return

        # 登录成功，查询角色列表
        self._set_state(LoginState.QUERYING_ROLES, "查询角色列表...")
        self._send_query_roles_request()

    def _send_query_roles_request(self):
        """发送查询角色列表请求"""
        self._start_timeout_timer()

        query_req = account_pb2.QueryRolesReq()
        query_req.account_id = self._account_id
        query_payload = query_req.SerializeToString()

        account_msg = base_pb2.AccountMsg()
        account_msg.account_id = self._account_id
        account_msg.msg_id = MSG_ID_QUERY_ROLES_REQ
        account_msg.payload = query_payload
        payload = account_msg.SerializeToString()

        self._connection.send_message(MSG_ID_QUERY_ROLES_REQ, payload)

    def _handle_query_roles_resp(self, payload: bytes):
        """处理查询角色列表响应"""
        self._cancel_timeout_timer()

        query_resp = account_pb2.QueryRolesResp()
        query_resp.ParseFromString(payload)

        if query_resp.code != 0:
            self._set_state(LoginState.ERROR, f"查询角色列表失败: {query_resp.msg}")
            return

        self._roles = list(query_resp.roles)

        if not self._roles:
            # 没有角色，自动创建
            self._set_state(LoginState.CREATING_ROLE, "创建角色中...")
            self._send_create_role_request()
        else:
            # 有角色，直接进入游戏
            first_role = self._roles[0]
            self._current_player_id = first_role.player_id
            self._current_server_id = first_role.server_id
            self._set_state(LoginState.ENTERING_GAME, "进入游戏中...")
            self._send_enter_game_request()

    def _send_create_role_request(self):
        """发送创建角色请求"""
        self._start_timeout_timer()

        create_req = account_pb2.CreateRoleReq()
        create_req.account_id = self._account_id
        create_req.server_id = 1  # 默认服务器 ID
        create_req.role_name = self._account_id  # 角色名 = 账号 ID
        create_payload = create_req.SerializeToString()

        account_msg = base_pb2.AccountMsg()
        account_msg.account_id = self._account_id
        account_msg.msg_id = MSG_ID_CREATE_ROLE_REQ
        account_msg.payload = create_payload
        payload = account_msg.SerializeToString()

        self._connection.send_message(MSG_ID_CREATE_ROLE_REQ, payload)

    def _handle_create_role_resp(self, payload: bytes):
        """处理创建角色响应"""
        self._cancel_timeout_timer()

        create_resp = account_pb2.CreateRoleResp()
        create_resp.ParseFromString(payload)

        if create_resp.code != 0:
            self._set_state(LoginState.ERROR, f"创建角色失败: {create_resp.msg}")
            return

        # 创建成功，使用返回的 player_id 进入游戏
        self._current_player_id = create_resp.player_id
        self._current_server_id = 1  # 默认服务器 ID
        self._set_state(LoginState.ENTERING_GAME, "进入游戏中...")
        self._send_enter_game_request()

    def _send_enter_game_request(self):
        """发送进入游戏请求"""
        self._start_timeout_timer()

        enter_req = player_pb2.EnterGameReq()
        enter_req.player_id = self._current_player_id
        enter_req.server_id = self._current_server_id
        enter_payload = enter_req.SerializeToString()

        account_msg = base_pb2.AccountMsg()
        account_msg.account_id = self._account_id
        account_msg.msg_id = MSG_ID_ENTER_GAME_REQ
        account_msg.payload = enter_payload
        payload = account_msg.SerializeToString()

        self._connection.send_message(MSG_ID_ENTER_GAME_REQ, payload)

    def _handle_enter_game_resp(self, payload: bytes):
        """处理进入游戏响应"""
        self._cancel_timeout_timer()

        enter_resp = player_pb2.EnterGameResp()
        enter_resp.ParseFromString(payload)

        if enter_resp.code != 0:
            self._set_state(LoginState.ERROR, f"进入游戏失败: {enter_resp.msg}")
            return

        # 进入游戏成功
        player_data = enter_resp.player_data
        self._set_state(LoginState.SUCCESS, "登录成功")

        # 调用成功回调
        if self._on_success:
            success_data = {
                "account_id": self._account_id,
                "player_id": player_data.player_id,
                "server_id": player_data.server_id,
                "role_name": player_data.role_name,
                "level": player_data.level,
                "exp": player_data.exp,
                "pos_x": player_data.pos_x,
                "pos_y": player_data.pos_y,
                "pos_z": player_data.pos_z,
                "scene_id": player_data.scene_id,
                "connection": self._connection,  # 传递连接对象供后续使用
            }

            # 提取能量数据
            if enter_resp.HasField("energy"):
                success_data["energy_current"] = enter_resp.energy.current
                success_data["energy_max"] = enter_resp.energy.max
            else:
                success_data["energy_current"] = 100
                success_data["energy_max"] = 100

            self._on_success(success_data)

    def _on_disconnect(self, reason: str):
        """处理连接断开"""
        self._cancel_timeout_timer()
        self._set_state(LoginState.ERROR, f"连接断开: {reason}")

    def _set_state(self, state: LoginState, message: str = ""):
        """设置登录状态"""
        with self._state_lock:
            self._state = state

        # 调用状态变化回调
        if self._on_state_change:
            self._on_state_change(state, message)

        # 如果是错误状态，调用错误回调
        if state == LoginState.ERROR and self._on_error:
            self._on_error(message)

    def _start_timeout_timer(self):
        """启动超时检测定时器"""
        self._step_start_time = time.time()
        self._cancel_timeout_timer()
        self._timeout_timer = threading.Timer(self.TIMEOUT_SECONDS, self._on_timeout)
        self._timeout_timer.daemon = True
        self._timeout_timer.start()

    def _cancel_timeout_timer(self):
        """取消超时检测定时器"""
        if self._timeout_timer:
            self._timeout_timer.cancel()
            self._timeout_timer = None

    def _on_timeout(self):
        """超时处理"""
        self._set_state(LoginState.ERROR, "操作超时")
        if self._connection:
            self._connection.disconnect()
            self._connection = None
