"""Standalone validation helpers for quest configurations."""

from __future__ import annotations

from typing import Dict, List, Set

from quest_node import QuestNode


def validate_quest_config(quests: Dict[str, QuestNode]) -> List[str]:
    """Run all validation checks and return a combined list of error strings.

    An empty list means the configuration is valid.

    Checks performed:
    1. Missing prerequisites (edge points to a non-existent quest id).
    2. Circular prerequisite chains.
    3. Isolated nodes (no incoming or outgoing edges at all).
    """
    errors: List[str] = []
    ids = set(quests.keys())

    # 1. Missing prerequisites
    for quest in quests.values():
        for prereq_id in quest.prerequisites:
            if prereq_id not in ids:
                errors.append(
                    f"Quest '{quest.id}' has missing prerequisite '{prereq_id}'."
                )

    # 2. Circular dependencies
    if has_circular_dependencies(quests):
        # Gather the detailed cycle information inline
        errors.extend(_collect_cycle_errors(quests))

    # 3. Isolated nodes
    isolated = find_isolated_nodes(quests)
    for qid in isolated:
        errors.append(f"Quest '{qid}' is isolated (no connections).")

    return errors


def has_circular_dependencies(quests: Dict[str, QuestNode]) -> bool:
    """Return ``True`` if the quest graph contains at least one cycle.

    Uses a standard three-colour DFS (white / grey / black).
    A node is coloured grey when it is first entered and black when its
    sub-tree has been fully explored.  Encountering a grey node means we
    have found a back-edge, i.e. a cycle.
    """
    ids = set(quests.keys())
    WHITE, GRAY, BLACK = 0, 1, 2
    colour: Dict[str, int] = {qid: WHITE for qid in ids}

    def _dfs(node_id: str) -> bool:
        colour[node_id] = GRAY
        for prereq_id in quests[node_id].prerequisites:
            if prereq_id not in ids:
                continue
            if colour[prereq_id] == GRAY:
                return True
            if colour[prereq_id] == WHITE:
                if _dfs(prereq_id):
                    return True
        colour[node_id] = BLACK
        return False

    for qid in ids:
        if colour[qid] == WHITE:
            if _dfs(qid):
                return True
    return False


def find_isolated_nodes(quests: Dict[str, QuestNode]) -> List[str]:
    """Return ids of nodes that have no incoming or outgoing edges.

    A node is considered *connected* if:
    - it lists any prerequisites, **or**
    - another node lists it as a prerequisite.

    Isolated nodes are only reported when there is more than one quest in
    the graph (a single-node graph is trivially valid).
    """
    ids = set(quests.keys())
    if len(ids) <= 1:
        return []

    connected: Set[str] = set()
    for quest in quests.values():
        if quest.prerequisites:
            connected.add(quest.id)
            for pid in quest.prerequisites:
                if pid in ids:
                    connected.add(pid)

    return [qid for qid in ids if qid not in connected]


# ------------------------------------------------------------------
# Internal helpers
# ------------------------------------------------------------------

def _collect_cycle_errors(quests: Dict[str, QuestNode]) -> List[str]:
    """Run DFS again and produce human-readable cycle descriptions."""
    ids = set(quests.keys())
    WHITE, GRAY, BLACK = 0, 1, 2
    colour: Dict[str, int] = {qid: WHITE for qid in ids}
    errors: List[str] = []

    def _dfs(node_id: str, path: List[str]) -> None:
        colour[node_id] = GRAY
        for prereq_id in quests[node_id].prerequisites:
            if prereq_id not in ids:
                continue
            if colour[prereq_id] == GRAY:
                cycle_path = path + [prereq_id]
                errors.append(
                    "Circular dependency detected: "
                    + " -> ".join(cycle_path)
                    + f" -> {prereq_id}."
                )
                continue
            if colour[prereq_id] == WHITE:
                _dfs(prereq_id, path + [prereq_id])
        colour[node_id] = BLACK

    for qid in ids:
        if colour[qid] == WHITE:
            _dfs(qid, [qid])

    return errors
