"""客户端任务数据模型

纯数据层，不依赖 pygame。从 quest_handler.py 同步数据，
为 quest_tracker 和 quest_panel 提供查询接口。
"""
import json
import logging
import os
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger("client.quest_data")


class _QuestConfig:
    """从 config/quest_data.json 加载并缓存任务配置。"""

    def __init__(self, config_path: Optional[str] = None):
        if config_path is None:
            config_path = os.path.join(
                os.path.dirname(__file__), '..', '..', 'config', 'quest_data.json'
            )
        self._quests: Dict[str, dict] = {}
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            self._quests = data.get('quests', {})
            logger.info(f"[QuestConfig]Loaded {len(self._quests)} quest configs from {config_path}")
        except FileNotFoundError:
            logger.warning(f"[QuestConfig]Config file not found: {config_path}")
        except Exception as e:
            logger.error(f"[QuestConfig]Failed to load config: {e}")

    def get_quest_config(self, quest_id: str) -> Optional[dict]:
        """获取指定任务的完整配置。"""
        return self._quests.get(quest_id)

    def get_objective_config(self, quest_id: str, obj_id: str) -> Optional[dict]:
        """获取指定任务目标的配置。"""
        quest_cfg = self._quests.get(quest_id)
        if not quest_cfg:
            return None
        for obj in quest_cfg.get('objectives', []):
            if obj.get('id') == obj_id:
                return obj
        return None


# 模块级单例，首次 import 时加载一次
_quest_config: Optional[_QuestConfig] = None


def _get_quest_config() -> _QuestConfig:
    """获取或创建模块级 _QuestConfig 单例。"""
    global _quest_config
    if _quest_config is None:
        _quest_config = _QuestConfig()
    return _quest_config


@dataclass
class ObjectiveInfo:
    """单个任务目标"""
    obj_id: str
    type: str            # "collect_item", "plant_crop", "harvest_crop", etc.
    target: str
    current: int
    required: int
    description: str


@dataclass
class RewardInfo:
    """任务奖励"""
    gold: int = 0
    exp: int = 0
    items: List[Tuple[str, int]] = field(default_factory=list)  # (item_id, count)


@dataclass
class QuestInfo:
    """单个任务信息"""
    quest_id: str
    name: str
    description: str
    state: str           # "in_progress", "completed", "available"
    objectives: List[ObjectiveInfo]
    rewards: RewardInfo
    trigger_npc: Optional[str] = None
    submit_npc: Optional[str] = None
    accept_time: int = 0


class QuestData:
    """客户端任务数据管理器

    从 quest_handler 同步任务数据，为 UI 组件提供查询接口。
    """

    def __init__(self, config: Optional[_QuestConfig] = None):
        self._quests: Dict[str, QuestInfo] = {}
        self._config = config if config is not None else _get_quest_config()

    # --- 数据同步 ---

    def sync_from_server(self, task_infos: dict) -> None:
        """登录时全量同步。task_infos 是 quest_handler.active_quests。"""
        self._quests.clear()
        for quest_id, task_info in task_infos.items():
            quest_info = self._convert_task_info(quest_id, task_info)
            if quest_info:
                self._quests[quest_id] = quest_info
        logger.info(f"[QuestData]Synced {len(self._quests)} quests from server")

    def update_progress(self, quest_id: str, progress: dict) -> None:
        """增量更新任务进度。progress 是 {obj_id: new_count}。"""
        quest = self._quests.get(quest_id)
        if not quest:
            logger.warning(f"[QuestData]Progress update for unknown quest: {quest_id}")
            return
        for obj in quest.objectives:
            if obj.obj_id in progress:
                obj.current = progress[obj.obj_id]
        logger.debug(f"[QuestData]Updated progress for quest {quest_id}")

    def add_quest(self, task_info: 'TaskInfo') -> None:
        """新增一个已接取的任务。"""
        quest_id = task_info.task_id
        quest_info = self._convert_task_info(quest_id, task_info)
        if quest_info:
            self._quests[quest_id] = quest_info
            logger.info(f"[QuestData]Added quest: {quest_id}")

    def remove_quest(self, quest_id: str) -> None:
        """移除任务（放弃或完成）。"""
        if quest_id in self._quests:
            del self._quests[quest_id]
            logger.info(f"[QuestData]Removed quest: {quest_id}")

    # --- 查询接口 ---

    def get_active_quests(self) -> List[QuestInfo]:
        """返回进行中的任务列表。"""
        return [q for q in self._quests.values() if q.state == "in_progress"]

    def get_available_quests(self) -> List[QuestInfo]:
        """返回可接取的任务列表。"""
        return [q for q in self._quests.values() if q.state == "available"]

    def get_completed_quests(self) -> List[QuestInfo]:
        """返回已完成的任务列表。"""
        return [q for q in self._quests.values() if q.state == "completed"]

    def get_tracked_quests(self, limit: int = 3) -> List[QuestInfo]:
        """返回追踪条显示的任务（前 N 个进行中的任务）。"""
        active = self.get_active_quests()
        return active[:limit]

    def get_quest(self, quest_id: str) -> Optional[QuestInfo]:
        """根据 quest_id 获取任务信息。"""
        return self._quests.get(quest_id)

    def get_npc_quest_status(self, npc_id: str) -> Optional[str]:
        """检查 NPC 是否有任务可交互。返回 '!' (可接取), '?' (可提交), None。"""
        for quest in self._quests.values():
            if quest.trigger_npc == npc_id and quest.state == "available":
                return "!"
            if quest.submit_npc == npc_id and quest.state == "in_progress":
                # 检查是否所有目标都完成
                all_done = all(o.current >= o.required for o in quest.objectives)
                if all_done:
                    return "?"
        return None

    @property
    def quest_count(self) -> int:
        """返回任务总数。"""
        return len(self._quests)

    # --- 内部转换 ---

    def _convert_task_info(self, quest_id: str, task_info: 'TaskInfo') -> Optional[QuestInfo]:
        """将 protobuf TaskInfo 转换为 QuestInfo，从配置填充 required 等字段。"""
        try:
            # 状态映射
            state_map = {0: "available", 1: "in_progress", 2: "in_progress",
                         3: "completed", 4: "failed"}
            state = state_map.get(task_info.state, "available")

            # 从 JSON 配置获取任务元数据
            quest_cfg = self._config.get_quest_config(quest_id)

            # 目标列表
            objectives = []
            for obj_id, count in task_info.progress.items():
                obj_cfg = self._config.get_objective_config(quest_id, obj_id)
                if obj_cfg:
                    objectives.append(ObjectiveInfo(
                        obj_id=obj_id,
                        type=obj_cfg.get('type', ''),
                        target=obj_cfg.get('target', ''),
                        current=count,
                        required=obj_cfg.get('count', 0),
                        description=obj_cfg.get('description', obj_id),
                    ))
                else:
                    objectives.append(ObjectiveInfo(
                        obj_id=obj_id,
                        type="",
                        target="",
                        current=count,
                        required=0,
                        description=obj_id,
                    ))

            # 奖励
            rewards = RewardInfo()
            if quest_cfg:
                rewards_cfg = quest_cfg.get('rewards', {})
                rewards = RewardInfo(
                    gold=rewards_cfg.get('gold', 0),
                    exp=rewards_cfg.get('exp', 0),
                    items=[(item['id'], item['count'])
                           for item in rewards_cfg.get('items', [])],
                )

            # trigger / submit NPC
            trigger_npc: Optional[str] = None
            submit_npc: Optional[str] = None
            if quest_cfg:
                trigger_cfg = quest_cfg.get('trigger', {})
                if trigger_cfg.get('type') == 'npc':
                    trigger_npc = trigger_cfg.get('npc_id')

            return QuestInfo(
                quest_id=quest_id,
                name=quest_cfg.get('name', quest_id) if quest_cfg else quest_id,
                description=quest_cfg.get('description', '') if quest_cfg else '',
                state=state,
                objectives=objectives,
                rewards=rewards,
                trigger_npc=trigger_npc,
                submit_npc=submit_npc,
                accept_time=task_info.accept_time,
            )
        except Exception as e:
            logger.error(f"[QuestData]Failed to convert task_info for {quest_id}: {e}")
            return None
