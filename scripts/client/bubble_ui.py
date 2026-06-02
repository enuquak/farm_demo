# scripts/client/bubble_ui.py
import pygame
from typing import Optional, Tuple
from .constants import BUBBLE_PADDING_X, BUBBLE_PADDING_Y, BUBBLE_OFFSET_Y, BUBBLE_FADE_TIME


class BubbleUI:
    """NPC 头顶气泡渲染。"""

    def __init__(self):
        self._font = None
        self._init_font()
        self._active_bubbles: dict = {}

    def _init_font(self):
        try:
            self._font = pygame.font.Font(None, 20)
        except Exception:
            self._font = pygame.font.SysFont('microsoftyahei', 16)

    def show_dots(self, npc_id: str):
        self._active_bubbles[npc_id] = {
            'type': 'dots', 'text': '...', 'timer': 0, 'permanent': True,
        }

    def hide_dots(self, npc_id: str):
        if npc_id in self._active_bubbles:
            data = self._active_bubbles[npc_id]
            if data.get('type') == 'dots':
                del self._active_bubbles[npc_id]

    def show_feedback(self, npc_id: str, text: str, duration: float = BUBBLE_FADE_TIME):
        self._active_bubbles[npc_id] = {
            'type': 'feedback', 'text': text, 'timer': 0,
            'duration': duration, 'permanent': False,
        }

    def update(self, dt: float):
        expired = []
        for npc_id, data in self._active_bubbles.items():
            if not data['permanent']:
                data['timer'] += dt
                if data['timer'] >= data['duration']:
                    expired.append(npc_id)
        for npc_id in expired:
            del self._active_bubbles[npc_id]

    def render(self, screen: pygame.Surface, npc_id: str,
               npc_screen_x: int, npc_screen_y: int):
        if npc_id not in self._active_bubbles:
            return
        data = self._active_bubbles[npc_id]
        text = data['text']
        text_surface = self._font.render(text, True, (40, 40, 40))
        tw, th = text_surface.get_size()
        bw = tw + BUBBLE_PADDING_X * 2
        bh = th + BUBBLE_PADDING_Y * 2
        bx = npc_screen_x - bw // 2
        by = npc_screen_y + BUBBLE_OFFSET_Y - bh
        alpha = 255
        if not data['permanent']:
            remaining = data['duration'] - data['timer']
            if remaining < 0.5:
                alpha = int(255 * (remaining / 0.5))
        bubble_surface = pygame.Surface((bw, bh), pygame.SRCALPHA)
        bubble_color = (255, 255, 255, alpha)
        pygame.draw.rect(bubble_surface, bubble_color, (0, 0, bw, bh), border_radius=8)
        pygame.draw.rect(bubble_surface, (100, 100, 100, alpha), (0, 0, bw, bh), width=1, border_radius=8)
        arrow_size = 6
        arrow_x = bw // 2
        arrow_y = bh
        pygame.draw.polygon(bubble_surface, bubble_color, [
            (arrow_x - arrow_size, arrow_y - 1),
            (arrow_x + arrow_size, arrow_y - 1),
            (arrow_x, arrow_y + arrow_size),
        ])
        screen.blit(bubble_surface, (bx, by))
        if alpha < 255:
            text_surface.set_alpha(alpha)
        screen.blit(text_surface, (bx + BUBBLE_PADDING_X, by + BUBBLE_PADDING_Y))

    def clear_all(self):
        self._active_bubbles.clear()
