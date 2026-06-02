# -*- coding: utf-8 -*-
"""Unit tests for the quest editor data model and validation."""
import json
import os
import sys
import tempfile

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'quest_editor'))

from quest_node import QuestNode, ObjectiveDef, QuestRewardDef, TriggerDef
from quest_data import QuestData
from validation import validate_quest_config, has_circular_dependencies


# ---------------------------------------------------------------------------
# QuestNode tests
# ---------------------------------------------------------------------------

class TestQuestNode:
    """Tests for the QuestNode data class."""

    def test_create_node(self):
        """Creating a node with id and name stores them correctly."""
        node = QuestNode(id="q1", name="First Quest")
        assert node.id == "q1"
        assert node.name == "First Quest"
        # Other fields should have sensible defaults
        assert node.objectives == []
        assert node.prerequisites == []

    def test_to_dict(self):
        """to_dict returns a dictionary with all expected keys."""
        node = QuestNode(id="q1", name="First Quest", description="desc")
        d = node.to_dict()
        expected_keys = {
            "id", "name", "description", "type", "trigger",
            "objectives", "rewards", "prerequisites",
            "branch_group", "x", "y",
        }
        assert set(d.keys()) == expected_keys
        assert d["id"] == "q1"
        assert d["name"] == "First Quest"
        assert d["description"] == "desc"

    def test_from_dict(self):
        """Round-trip: to_dict then from_dict reproduces the original node."""
        original = QuestNode(
            id="q2",
            name="Second Quest",
            description="a longer description",
            type="main",
            branch_group="act1",
            x=10.0,
            y=20.0,
        )
        rebuilt = QuestNode.from_dict(original.to_dict())
        assert rebuilt.id == original.id
        assert rebuilt.name == original.name
        assert rebuilt.description == original.description
        assert rebuilt.type == original.type
        assert rebuilt.branch_group == original.branch_group
        assert rebuilt.x == pytest.approx(original.x)
        assert rebuilt.y == pytest.approx(original.y)

    def test_from_dict_with_objectives(self):
        """Objectives are correctly parsed during from_dict round-trip."""
        obj = ObjectiveDef(id="o1", type="kill_enemy", target="slime", count=5, description="Kill 5 slimes")
        node = QuestNode(
            id="q3",
            name="Hunt",
            objectives=[obj],
            rewards=QuestRewardDef(gold=100, exp=50),
        )
        rebuilt = QuestNode.from_dict(node.to_dict())
        assert len(rebuilt.objectives) == 1
        assert rebuilt.objectives[0].id == "o1"
        assert rebuilt.objectives[0].type == "kill_enemy"
        assert rebuilt.objectives[0].target == "slime"
        assert rebuilt.objectives[0].count == 5
        assert rebuilt.rewards.gold == 100
        assert rebuilt.rewards.exp == 50


# ---------------------------------------------------------------------------
# QuestData tests
# ---------------------------------------------------------------------------

class TestQuestData:
    """Tests for the QuestData manager."""

    def test_add_quest(self):
        """Adding a quest makes it retrievable by id."""
        data = QuestData()
        q = data.add_quest("q1")
        assert q.id == "q1"
        assert data.get_quest("q1") is q
        assert len(data.quests) == 1

    def test_remove_quest(self):
        """Removing a quest also cleans up references in other quests."""
        data = QuestData()
        data.add_quest("q1")
        data.add_quest("q2")
        data.add_quest("q3")

        # q2 depends on q1; q3 depends on q1
        data.connect_quests("q1", "q2")
        data.connect_quests("q1", "q3")

        # q1 also lists q2 in its unlock_quests reward
        data.get_quest("q1").rewards.unlock_quests.append("q2")

        data.remove_quest("q1")

        assert data.get_quest("q1") is None
        # q2 and q3 should no longer reference q1
        assert "q1" not in data.get_quest("q2").prerequisites
        assert "q1" not in data.get_quest("q3").prerequisites
        # q2's unlock_quests referencing q1 should also be cleaned
        # (unlock_quests cleanup operates on remaining quests)
        assert len(data.quests) == 2

    def test_connect_quests(self):
        """Connecting two quests sets up the prerequisite relationship."""
        data = QuestData()
        data.add_quest("q1")
        data.add_quest("q2")
        data.connect_quests("q1", "q2")

        q2 = data.get_quest("q2")
        assert "q1" in q2.prerequisites

    def test_disconnect_quests(self):
        """Disconnecting removes the prerequisite link."""
        data = QuestData()
        data.add_quest("q1")
        data.add_quest("q2")
        data.connect_quests("q1", "q2")
        assert "q1" in data.get_quest("q2").prerequisites

        data.disconnect_quests("q1", "q2")
        assert "q1" not in data.get_quest("q2").prerequisites

    def test_save_and_load(self):
        """Save to a temp file and load back -- data must match."""
        data = QuestData()
        data.add_quest("q1")
        data.add_quest("q2")
        data.connect_quests("q1", "q2")
        data.get_quest("q1").description = "hello"

        tmp = tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False,
        )
        tmp.close()
        try:
            ok, msg = data.save(tmp.name)
            assert ok, msg

            data2 = QuestData()
            ok2, msg2 = data2.load(tmp.name)
            assert ok2, msg2

            assert set(data2.quests.keys()) == {"q1", "q2"}
            assert data2.get_quest("q1").description == "hello"
            assert "q1" in data2.get_quest("q2").prerequisites
        finally:
            os.unlink(tmp.name)


# ---------------------------------------------------------------------------
# Validation tests
# ---------------------------------------------------------------------------

class TestValidation:
    """Tests for standalone validation helpers."""

    def test_no_circular(self):
        """A linear chain q1->q2->q3 has no circular dependencies."""
        quests = {
            "q1": QuestNode(id="q1", name="A"),
            "q2": QuestNode(id="q2", name="B", prerequisites=["q1"]),
            "q3": QuestNode(id="q3", name="C", prerequisites=["q2"]),
        }
        assert has_circular_dependencies(quests) is False
        errors = validate_quest_config(quests)
        # Should have no circular-related errors
        assert not any("Circular" in e for e in errors)

    def test_circular_detected(self):
        """A cycle q1->q2->q1 is detected."""
        quests = {
            "q1": QuestNode(id="q1", name="A", prerequisites=["q2"]),
            "q2": QuestNode(id="q2", name="B", prerequisites=["q1"]),
        }
        assert has_circular_dependencies(quests) is True
        errors = validate_quest_config(quests)
        assert any("Circular" in e for e in errors)

    def test_missing_prerequisite(self):
        """A quest referencing a non-existent prerequisite is flagged."""
        quests = {
            "q1": QuestNode(id="q1", name="A", prerequisites=["ghost"]),
        }
        errors = validate_quest_config(quests)
        assert any("missing prerequisite" in e and "ghost" in e for e in errors)

    def test_isolated_node(self):
        """A node with no connections in a multi-node graph is flagged."""
        quests = {
            "q1": QuestNode(id="q1", name="A", prerequisites=["q2"]),
            "q2": QuestNode(id="q2", name="B"),
            "q3": QuestNode(id="q3", name="C"),  # isolated
        }
        errors = validate_quest_config(quests)
        assert any("isolated" in e and "q3" in e for e in errors)
