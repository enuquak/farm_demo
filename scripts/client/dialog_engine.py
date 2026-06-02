# scripts/client/dialog_engine.py
import json
import os
from typing import Dict, List, Optional, Any
from enum import Enum


def _load_json(relative_path: str) -> dict:
    base_dir = os.path.dirname(os.path.abspath(__file__))
    full_path = os.path.join(base_dir, relative_path)
    with open(full_path, 'r', encoding='utf-8') as f:
        return json.load(f)


class DialogState(Enum):
    IDLE = "idle"
    TYPING = "typing"
    WAITING_INPUT = "waiting_input"
    CHOOSING = "choosing"


class DialogEngine:
    """对话树状态机，驱动对话流程。"""

    def __init__(self):
        self._dialog_data: dict = _load_json('data/npc_dialog.json')
        self._gift_data: dict = _load_json('data/npc_gifts.json')
        self._state = DialogState.IDLE
        self._current_npc_id: Optional[str] = None
        self._current_node_key: Optional[str] = None
        self._current_node: Optional[dict] = None
        self._current_line_index: int = 0
        self._current_text: str = ""
        self._displayed_chars: int = 0
        self._char_timer: float = 0.0
        self._responses: List[dict] = []
        self._selected_option: int = 0
        self._pending_effects: List[dict] = []
        self._text_speed: float = 30.0

    @property
    def state(self) -> DialogState:
        return self._state

    @property
    def is_active(self) -> bool:
        return self._state != DialogState.IDLE

    @property
    def current_npc_id(self) -> Optional[str]:
        return self._current_npc_id

    @property
    def displayed_text(self) -> str:
        return self._current_text[:self._displayed_chars]

    @property
    def is_text_complete(self) -> bool:
        return self._displayed_chars >= len(self._current_text)

    @property
    def current_speaker(self) -> Optional[str]:
        if self._current_node and self._current_line_index < len(self._current_node.get('lines', [])):
            line = self._current_node['lines'][self._current_line_index]
            return line.get('speaker', self._current_npc_id)
        return self._current_npc_id

    @property
    def responses(self) -> List[dict]:
        return self._responses

    @property
    def selected_option(self) -> int:
        return self._selected_option

    @property
    def pending_effects(self) -> List[dict]:
        effects = self._pending_effects.copy()
        self._pending_effects.clear()
        return effects

    def start_dialog(self, npc_id: str, affection: int = 0,
                     time_slot: str = "08:00", season: str = "spring") -> bool:
        if self._state != DialogState.IDLE:
            return False
        npc_dialog = self._dialog_data.get(npc_id)
        if not npc_dialog:
            return False
        context = {'affection': affection, 'time_slot': time_slot, 'season': season}
        node_key = self._select_node(npc_dialog, context)
        if not node_key:
            return False
        self._current_npc_id = npc_id
        self._current_node_key = node_key
        self._current_node = npc_dialog[node_key]
        self._current_line_index = 0
        self._responses = []
        self._selected_option = 0
        self._pending_effects = []
        self._start_line()
        self._state = DialogState.TYPING
        return True

    def _select_node(self, npc_dialog: dict, context: dict) -> Optional[str]:
        candidates = []
        for key, node in npc_dialog.items():
            if self._evaluate_node_conditions(node, context):
                min_affection = 0
                condition = node.get('condition', {})
                if 'affection' in condition:
                    min_affection = condition['affection'].get('min', 0)
                candidates.append((min_affection, key))
        if not candidates:
            return None
        candidates.sort(key=lambda x: x[0], reverse=True)
        return candidates[0][1]

    def _evaluate_node_conditions(self, node: dict, context: dict) -> bool:
        condition = node.get('condition', {})
        if 'affection' in condition:
            aff_cond = condition['affection']
            affection = context.get('affection', 0)
            if 'min' in aff_cond and affection < aff_cond['min']:
                return False
            if 'max' in aff_cond and affection > aff_cond['max']:
                return False
        time_cond = node.get('time_condition', {})
        if time_cond:
            if not self._match_time_condition(context.get('time_slot', '08:00'), time_cond):
                return False
        season = node.get('season')
        if season and context.get('season') != season:
            return False
        return True

    def _match_time_condition(self, time_slot: str, condition: dict) -> bool:
        h, m = map(int, time_slot.split(':'))
        minutes = h * 60 + m
        time_periods = {
            'morning': (360, 720),
            'afternoon': (720, 1080),
            'evening': (1080, 1440),
            'night': (0, 360),
        }
        for period, required in condition.items():
            if required and period in time_periods:
                start, end = time_periods[period]
                if not (start <= minutes < end):
                    return False
        return True

    def _start_line(self):
        lines = self._current_node.get('lines', [])
        if self._current_line_index < len(lines):
            self._current_text = lines[self._current_line_index]['text']
            self._displayed_chars = 0
            self._char_timer = 0.0

    def update(self, dt: float):
        if self._state == DialogState.TYPING:
            self._char_timer += dt
            chars_to_add = int(self._char_timer * self._text_speed)
            if chars_to_add > 0:
                self._char_timer -= chars_to_add / self._text_speed
                self._displayed_chars = min(self._displayed_chars + chars_to_add, len(self._current_text))

    def advance(self) -> bool:
        if self._state == DialogState.TYPING:
            self._displayed_chars = len(self._current_text)
            self._state = DialogState.WAITING_INPUT
            return True
        if self._state == DialogState.WAITING_INPUT:
            return self._advance_line()
        if self._state == DialogState.CHOOSING:
            return self._confirm_choice()
        return False

    def _advance_line(self) -> bool:
        self._current_line_index += 1
        lines = self._current_node.get('lines', [])
        if self._current_line_index < len(lines):
            self._start_line()
            self._state = DialogState.TYPING
            return True
        effect = self._current_node.get('effect')
        if effect:
            self._pending_effects.append(effect)
        responses = self._current_node.get('responses', [])
        if responses:
            self._responses = responses
            self._selected_option = 0
            self._state = DialogState.CHOOSING
            return True
        next_node = self._current_node.get('next')
        if next_node:
            return self._jump_to_node(next_node)
        self._close_dialog()
        return False

    def _jump_to_node(self, node_key: str) -> bool:
        npc_dialog = self._dialog_data.get(self._current_npc_id, {})
        if node_key not in npc_dialog:
            self._close_dialog()
            return False
        self._current_node_key = node_key
        self._current_node = npc_dialog[node_key]
        self._current_line_index = 0
        self._start_line()
        self._state = DialogState.TYPING
        return True

    def select_option(self, direction: int):
        if self._state == DialogState.CHOOSING and self._responses:
            self._selected_option = (self._selected_option + direction) % len(self._responses)

    def _confirm_choice(self) -> bool:
        if not self._responses:
            self._close_dialog()
            return False
        chosen = self._responses[self._selected_option]
        next_node = chosen.get('next')
        if next_node:
            return self._jump_to_node(next_node)
        self._close_dialog()
        return False

    def _close_dialog(self):
        self._state = DialogState.IDLE
        self._current_npc_id = None
        self._current_node_key = None
        self._current_node = None
        self._current_line_index = 0
        self._current_text = ""
        self._displayed_chars = 0
        self._responses = []
        self._selected_option = 0

    def get_gift_reaction(self, npc_id: str, item_id: str) -> dict:
        gift_data = self._gift_data.get(npc_id, {})
        for category in ['loved', 'liked', 'disliked']:
            tier = gift_data.get(category, {})
            if item_id in tier.get('items', []):
                return {
                    'category': category,
                    'affection': tier.get('affection', 0),
                    'dialog_node': f'gift_{category}',
                }
        neutral = gift_data.get('neutral', {})
        return {
            'category': 'neutral',
            'affection': neutral.get('affection', 1),
            'dialog_node': 'gift_neutral',
        }

    def start_gift_dialog(self, npc_id: str, reaction: dict) -> bool:
        if self._state != DialogState.IDLE:
            return False
        npc_dialog = self._dialog_data.get(npc_id, {})
        node_key = reaction.get('dialog_node', 'gift_neutral')
        if node_key not in npc_dialog:
            return False
        self._current_npc_id = npc_id
        self._current_node_key = node_key
        self._current_node = npc_dialog[node_key]
        self._current_line_index = 0
        self._responses = []
        self._pending_effects = [{'affection': reaction.get('affection', 0)}]
        self._start_line()
        self._state = DialogState.TYPING
        return True
