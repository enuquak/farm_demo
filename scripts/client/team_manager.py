"""
客户端组队管理器
管理队伍状态、发送组队请求、处理组队回调。
"""
import logging
from typing import List, Optional, Callable
from dataclasses import dataclass, field

from .message_ids import (
    MSG_ID_TEAM_CREATE_REQ, MSG_ID_TEAM_CREATE_RESP,
    MSG_ID_TEAM_DISBAND_REQ, MSG_ID_TEAM_DISBAND_RESP,
    MSG_ID_TEAM_INVITE_REQ, MSG_ID_TEAM_INVITE_RESP,
    MSG_ID_TEAM_INVITE_NOTIFY,
    MSG_ID_TEAM_ACCEPT_REQ, MSG_ID_TEAM_ACCEPT_RESP,
    MSG_ID_TEAM_REJECT_REQ, MSG_ID_TEAM_REJECT_RESP,
    MSG_ID_TEAM_LEAVE_REQ, MSG_ID_TEAM_LEAVE_RESP,
    MSG_ID_TEAM_KICK_REQ, MSG_ID_TEAM_KICK_RESP,
    MSG_ID_TEAM_INFO_REQ, MSG_ID_TEAM_INFO_RESP,
    MSG_ID_TEAM_MEMBER_UPDATE, MSG_ID_TEAM_LEADER_CHANGE,
    MSG_ID_TEAM_STATUS_UPDATE,
)

logger = logging.getLogger("client.team_manager")


@dataclass
class TeamMember:
    """队伍成员信息"""
    player_id: int = 0
    role_name: str = ""
    level: int = 0
    online: bool = True
    is_leader: bool = False
    current_hp: int = 0
    max_hp: int = 0


@dataclass
class TeamInviteNotify:
    """组队邀请通知"""
    team_id: int = 0
    inviter_id: int = 0
    inviter_name: str = ""
    inviter_level: int = 0


class TeamManager:
    """客户端组队管理器"""

    def __init__(self, connection, player_id: int):
        self._connection = connection
        self._player_id = player_id
        self._team_id: int = 0
        self._leader_id: int = 0
        self._members: List[TeamMember] = []
        self._pending_invites: List[TeamInviteNotify] = []
        self._status: int = 0  # 0=idle, 1=in_cave
        self._cave_level: int = 0

        # Callbacks
        self.on_team_created: Optional[Callable[[int], None]] = None
        self.on_team_disbanded: Optional[Callable[[], None]] = None
        self.on_member_update: Optional[Callable[[], None]] = None
        self.on_leader_change: Optional[Callable[[int], None]] = None
        self.on_invite_received: Optional[Callable[[TeamInviteNotify], None]] = None
        self.on_error: Optional[Callable[[int, str], None]] = None

    @property
    def in_team(self) -> bool:
        return self._team_id != 0

    @property
    def is_leader(self) -> bool:
        return self._leader_id == self._player_id

    @property
    def team_id(self) -> int:
        return self._team_id

    @property
    def members(self) -> List[TeamMember]:
        return self._members

    @property
    def member_count(self) -> int:
        return len(self._members)

    @property
    def pending_invites(self) -> List[TeamInviteNotify]:
        return self._pending_invites

    def create_team(self) -> None:
        """创建队伍"""
        logger.info("Creating team")
        self._connection.send_message(MSG_ID_TEAM_CREATE_REQ, b'')

    def disband_team(self) -> None:
        """解散队伍（队长）"""
        if not self.in_team:
            return
        logger.info(f"Disbanding team {self._team_id}")
        self._connection.send_message(MSG_ID_TEAM_DISBAND_REQ, b'')

    def invite_player(self, target_id: int) -> None:
        """邀请玩家组队"""
        if not self.in_team:
            return
        logger.info(f"Inviting player {target_id} to team {self._team_id}")
        # Encode target_id as uint64
        data = target_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_INVITE_REQ, data)

    def accept_invite(self, team_id: int) -> None:
        """接受组队邀请"""
        logger.info(f"Accepting invite to team {team_id}")
        data = team_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_ACCEPT_REQ, data)

    def reject_invite(self, team_id: int) -> None:
        """拒绝组队邀请"""
        logger.info(f"Rejecting invite to team {team_id}")
        data = team_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_REJECT_REQ, data)

    def leave_team(self) -> None:
        """离开队伍"""
        if not self.in_team:
            return
        logger.info(f"Leaving team {self._team_id}")
        self._connection.send_message(MSG_ID_TEAM_LEAVE_REQ, b'')

    def kick_member(self, target_id: int) -> None:
        """踢出成员（队长）"""
        if not self.in_team or not self.is_leader:
            return
        logger.info(f"Kicking player {target_id} from team {self._team_id}")
        data = target_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_KICK_REQ, data)

    def request_team_info(self) -> None:
        """查询队伍信息"""
        self._connection.send_message(MSG_ID_TEAM_INFO_REQ, b'')

    # --- Message handlers (called by NetworkDispatcher) ---

    def on_team_create_resp(self, data: bytes) -> None:
        """处理创建队伍响应"""
        import team_pb2
        resp = team_pb2.TeamCreateResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.team_id
            logger.info(f"Team created: {self._team_id}")
            if self.on_team_created:
                self.on_team_created(self._team_id)
        else:
            logger.warning(f"Create team failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "创建队伍失败")

    def on_team_disband_resp(self, data: bytes) -> None:
        """处理解散队伍响应"""
        import team_pb2
        resp = team_pb2.TeamDisbandResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._clear_team()
            logger.info("Team disbanded")
            if self.on_team_disbanded:
                self.on_team_disbanded()

    def on_team_invite_resp(self, data: bytes) -> None:
        """处理邀请响应"""
        import team_pb2
        resp = team_pb2.TeamInviteResp()
        resp.ParseFromString(data)
        if resp.code != 0:
            logger.warning(f"Invite failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "邀请失败")

    def on_team_invite_notify(self, data: bytes) -> None:
        """处理收到邀请通知"""
        import team_pb2
        notify = team_pb2.TeamInviteNotify()
        notify.ParseFromString(data)
        invite = TeamInviteNotify(
            team_id=notify.team_id,
            inviter_id=notify.inviter_id,
            inviter_name=notify.inviter_name,
            inviter_level=notify.inviter_level,
        )
        self._pending_invites.append(invite)
        logger.info(f"Received invite from {notify.inviter_name} to team {notify.team_id}")
        if self.on_invite_received:
            self.on_invite_received(invite)

    def on_team_accept_resp(self, data: bytes) -> None:
        """处理接受邀请响应"""
        import team_pb2
        resp = team_pb2.TeamAcceptResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.members[0].team_id if resp.members else 0
            self._update_members(resp.members)
            # Remove pending invite
            self._pending_invites = [
                i for i in self._pending_invites if i.team_id != self._team_id
            ]
            logger.info(f"Joined team {self._team_id}")
            if self.on_member_update:
                self.on_member_update()

    def on_team_reject_resp(self, data: bytes) -> None:
        """处理拒绝邀请响应"""
        import team_pb2
        resp = team_pb2.TeamRejectResp()
        resp.ParseFromString(data)
        # Remove from pending
        # team_id not in resp, clean up based on accept req context

    def on_team_leave_resp(self, data: bytes) -> None:
        """处理离开队伍响应"""
        import team_pb2
        resp = team_pb2.TeamLeaveResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._clear_team()
            logger.info("Left team")

    def on_team_kick_resp(self, data: bytes) -> None:
        """处理踢出响应"""
        import team_pb2
        resp = team_pb2.TeamKickResp()
        resp.ParseFromString(data)
        if resp.code != 0:
            logger.warning(f"Kick failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "踢出失败")

    def on_team_info_resp(self, data: bytes) -> None:
        """处理队伍信息响应"""
        import team_pb2
        resp = team_pb2.TeamInfoResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.team_id
            self._leader_id = resp.leader_id
            self._status = resp.status
            self._cave_level = resp.cave_level
            self._update_members(resp.members)
            logger.info(f"Team info: id={self._team_id}, members={len(self._members)}")

    def on_team_member_update(self, data: bytes) -> None:
        """处理成员变更通知"""
        import team_pb2
        notify = team_pb2.TeamMemberUpdate()
        notify.ParseFromString(data)
        self._update_members(notify.all_members)
        logger.info(f"Member update: type={notify.update_type}")
        if self.on_member_update:
            self.on_member_update()

    def on_team_leader_change(self, data: bytes) -> None:
        """处理队长变更通知"""
        import team_pb2
        notify = team_pb2.TeamLeaderChange()
        notify.ParseFromString(data)
        self._leader_id = notify.new_leader_id
        # Update member is_leader flags
        for m in self._members:
            m.is_leader = (m.player_id == self._leader_id)
        logger.info(f"Leader changed to {notify.new_leader_id}")
        if self.on_leader_change:
            self.on_leader_change(notify.new_leader_id)

    def on_team_status_update(self, data: bytes) -> None:
        """处理队伍状态变更"""
        import team_pb2
        notify = team_pb2.TeamStatusUpdate()
        notify.ParseFromString(data)
        self._status = notify.status
        self._cave_level = notify.cave_level

    # --- Internal helpers ---

    def _update_members(self, proto_members) -> None:
        """从 proto 成员列表更新内存"""
        self._members = []
        for m in proto_members:
            member = TeamMember(
                player_id=m.player_id,
                role_name=m.role_name,
                level=m.level,
                online=m.online,
                is_leader=m.is_leader,
                current_hp=m.current_hp,
                max_hp=m.max_hp,
            )
            self._members.append(member)
        if self._members:
            self._leader_id = next((m.player_id for m in self._members if m.is_leader), 0)

    def _clear_team(self) -> None:
        """清除队伍状态"""
        self._team_id = 0
        self._leader_id = 0
        self._members.clear()
        self._status = 0
        self._cave_level = 0
