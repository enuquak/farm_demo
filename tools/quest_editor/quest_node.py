"""Quest node data model for the quest editor."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List


@dataclass
class ObjectiveDef:
    """A single quest objective."""

    id: str = ""
    type: str = "collect_item"
    target: str = ""
    count: int = 1
    description: str = ""

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "type": self.type,
            "target": self.target,
            "count": self.count,
            "description": self.description,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "ObjectiveDef":
        return cls(
            id=data.get("id", ""),
            type=data.get("type", "collect_item"),
            target=data.get("target", ""),
            count=data.get("count", 1),
            description=data.get("description", ""),
        )


@dataclass
class ItemReward:
    """An item given as a reward."""

    id: str = ""
    count: int = 1

    def to_dict(self) -> dict:
        return {"id": self.id, "count": self.count}

    @classmethod
    def from_dict(cls, data: dict) -> "ItemReward":
        return cls(id=data.get("id", ""), count=data.get("count", 1))


@dataclass
class QuestRewardDef:
    """All rewards granted when a quest is completed."""

    items: List[ItemReward] = field(default_factory=list)
    gold: int = 0
    exp: int = 0
    unlock_quests: List[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "items": [item.to_dict() for item in self.items],
            "gold": self.gold,
            "exp": self.exp,
            "unlock_quests": list(self.unlock_quests),
        }

    @classmethod
    def from_dict(cls, data: dict) -> "QuestRewardDef":
        return cls(
            items=[ItemReward.from_dict(i) for i in data.get("items", [])],
            gold=data.get("gold", 0),
            exp=data.get("exp", 0),
            unlock_quests=list(data.get("unlock_quests", [])),
        )


@dataclass
class TriggerDef:
    """Defines how a quest becomes available."""

    type: str = "auto"
    condition: str = ""
    npc_id: str = ""
    dialogue: str = ""

    def to_dict(self) -> dict:
        return {
            "type": self.type,
            "condition": self.condition,
            "npc_id": self.npc_id,
            "dialogue": self.dialogue,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "TriggerDef":
        return cls(
            type=data.get("type", "auto"),
            condition=data.get("condition", ""),
            npc_id=data.get("npc_id", ""),
            dialogue=data.get("dialogue", ""),
        )


@dataclass
class QuestNode:
    """A single quest in the quest graph."""

    id: str = ""
    name: str = ""
    description: str = ""
    type: str = ""
    trigger: TriggerDef = field(default_factory=TriggerDef)
    objectives: List[ObjectiveDef] = field(default_factory=list)
    rewards: QuestRewardDef = field(default_factory=QuestRewardDef)
    prerequisites: List[str] = field(default_factory=list)
    branch_group: str = ""
    x: float = 0.0
    y: float = 0.0

    # ------------------------------------------------------------------
    # Serialisation
    # ------------------------------------------------------------------

    def to_dict(self) -> dict:
        """Return a JSON-compatible dictionary."""
        return {
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "type": self.type,
            "trigger": self.trigger.to_dict(),
            "objectives": [o.to_dict() for o in self.objectives],
            "rewards": self.rewards.to_dict(),
            "prerequisites": list(self.prerequisites),
            "branch_group": self.branch_group,
            "x": self.x,
            "y": self.y,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "QuestNode":
        """Create a QuestNode from a dictionary."""
        return cls(
            id=data.get("id", ""),
            name=data.get("name", ""),
            description=data.get("description", ""),
            type=data.get("type", ""),
            trigger=TriggerDef.from_dict(data.get("trigger", {})),
            objectives=[ObjectiveDef.from_dict(o) for o in data.get("objectives", [])],
            rewards=QuestRewardDef.from_dict(data.get("rewards", {})),
            prerequisites=list(data.get("prerequisites", [])),
            branch_group=data.get("branch_group", ""),
            x=float(data.get("x", 0.0)),
            y=float(data.get("y", 0.0)),
        )
