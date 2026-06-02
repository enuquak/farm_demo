"""JSON persistence and validation for the quest editor."""

from __future__ import annotations

import json
from typing import Dict, List, Optional, Tuple

from quest_node import QuestNode


class QuestData:
    """Manages the full set of quests and provides load / save / validate."""

    def __init__(self) -> None:
        self.quests: Dict[str, QuestNode] = {}
        self.branch_groups: Dict[str, dict] = {}
        self.file_path: Optional[str] = None
        self.modified: bool = False

    # ------------------------------------------------------------------
    # File operations
    # ------------------------------------------------------------------

    def new(self) -> None:
        """Clear all data and start fresh."""
        self.quests.clear()
        self.branch_groups.clear()
        self.file_path = None
        self.modified = False

    def load(self, file_path: str) -> Tuple[bool, str]:
        """Load quest data from a JSON file.

        Returns (success, message).
        """
        try:
            with open(file_path, "r", encoding="utf-8") as fh:
                raw = json.load(fh)
        except (OSError, json.JSONDecodeError) as exc:
            return False, f"Failed to load '{file_path}': {exc}"

        self.quests.clear()
        for quest_id, quest_dict in raw.get("quests", {}).items():
            self.quests[quest_id] = QuestNode.from_dict(quest_dict)

        self.branch_groups = dict(raw.get("branch_groups", {}))
        self.file_path = file_path
        self.modified = False
        return True, f"Loaded {len(self.quests)} quest(s) from '{file_path}'."

    def save(self, file_path: Optional[str] = None) -> Tuple[bool, str]:
        """Save quest data to a JSON file.

        Returns (success, message).
        """
        target = file_path or self.file_path
        if target is None:
            return False, "No file path specified."

        payload = {
            "quests": {qid: q.to_dict() for qid, q in self.quests.items()},
            "branch_groups": dict(self.branch_groups),
        }

        try:
            with open(target, "w", encoding="utf-8") as fh:
                json.dump(payload, fh, indent=2, ensure_ascii=False)
        except OSError as exc:
            return False, f"Failed to save '{target}': {exc}"

        self.file_path = target
        self.modified = False
        return True, f"Saved {len(self.quests)} quest(s) to '{target}'."

    # ------------------------------------------------------------------
    # Quest manipulation
    # ------------------------------------------------------------------

    def add_quest(self, quest_id: str, x: float = 0.0, y: float = 0.0) -> QuestNode:
        """Create a new quest and add it to the data set.

        If *quest_id* already exists the existing node is returned unchanged.
        """
        if quest_id in self.quests:
            return self.quests[quest_id]

        quest = QuestNode(id=quest_id, name=quest_id, x=x, y=y)
        self.quests[quest_id] = quest
        self.modified = True
        return quest

    def remove_quest(self, quest_id: str) -> None:
        """Remove a quest and clean up all references to it."""
        self.quests.pop(quest_id, None)

        # Remove from other quests' prerequisites
        for quest in self.quests.values():
            if quest_id in quest.prerequisites:
                quest.prerequisites.remove(quest_id)

        # Remove from unlock_quests in rewards
        for quest in self.quests.values():
            if quest_id in quest.rewards.unlock_quests:
                quest.rewards.unlock_quests.remove(quest_id)

        self.modified = True

    def connect_quests(self, from_id: str, to_id: str) -> None:
        """Add *from_id* as a prerequisite of *to_id*."""
        target = self.quests.get(to_id)
        if target is None:
            return
        if from_id not in target.prerequisites:
            target.prerequisites.append(from_id)
            self.modified = True

    def disconnect_quests(self, from_id: str, to_id: str) -> None:
        """Remove *from_id* from the prerequisites of *to_id*."""
        target = self.quests.get(to_id)
        if target is None:
            return
        if from_id in target.prerequisites:
            target.prerequisites.remove(from_id)
            self.modified = True

    def get_quest(self, quest_id: str) -> Optional[QuestNode]:
        """Return the quest with the given id, or ``None``."""
        return self.quests.get(quest_id)

    # ------------------------------------------------------------------
    # Validation
    # ------------------------------------------------------------------

    def validate(self) -> List[str]:
        """Return a list of validation error strings (empty == valid).

        Checks performed:
        - Missing prerequisites (edge points to non-existent quest)
        - Circular prerequisite chains
        - Isolated nodes (no incoming or outgoing edges)
        """
        errors: List[str] = []
        ids = set(self.quests.keys())

        # 1. Missing prerequisites
        for quest in self.quests.values():
            for prereq_id in quest.prerequisites:
                if prereq_id not in ids:
                    errors.append(
                        f"Quest '{quest.id}' has missing prerequisite '{prereq_id}'."
                    )

        # 2. Circular dependencies (DFS)
        WHITE, GRAY, BLACK = 0, 1, 2
        colour: Dict[str, int] = {qid: WHITE for qid in ids}

        def _dfs(node_id: str, path: List[str]) -> bool:
            """Return True if a cycle is detected."""
            colour[node_id] = GRAY
            for prereq_id in self.quests[node_id].prerequisites:
                if prereq_id not in ids:
                    continue
                if colour.get(prereq_id) == GRAY:
                    cycle_path = path + [prereq_id]
                    errors.append(
                        "Circular dependency detected: "
                        + " -> ".join(cycle_path)
                        + f" -> {prereq_id}."
                    )
                    return True
                if colour.get(prereq_id) == WHITE:
                    if _dfs(prereq_id, path + [prereq_id]):
                        return True
            colour[node_id] = BLACK
            return False

        for qid in ids:
            if colour[qid] == WHITE:
                _dfs(qid, [qid])

        # 3. Isolated nodes (no edges at all)
        has_edge: Dict[str, bool] = {qid: False for qid in ids}
        for quest in self.quests.values():
            if quest.prerequisites:
                has_edge[quest.id] = True
            for prereq_id in quest.prerequisites:
                if prereq_id in ids:
                    has_edge[prereq_id] = True

        for qid, connected in has_edge.items():
            if not connected and len(ids) > 1:
                errors.append(f"Quest '{qid}' is isolated (no connections).")

        return errors
