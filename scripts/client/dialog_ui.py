# scripts/client/dialog_ui.py
import pygame
from typing import Optional, List, Tuple
from .constants import (
    DIALOG_BOX_WIDTH_RATIO, DIALOG_BOX_HEIGHT, DIALOG_BOX_MARGIN_BOTTOM,
    DIALOG_BOX_ALPHA, DIALOG_PORTRAIT_SIZE, DIALOG_TEXT_SPEED,
    DIALOG_OPTION_GAP, TILE_SIZE, ZOOM_FACTOR
)


class DialogUI:
    """底部对话框 UI，渲染 NPC 头像、名字、台词和选项。"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        box_w = int(screen_width * DIALOG_BOX_WIDTH_RATIO)
        box_h = DIALOG_BOX_HEIGHT
        box_x = (screen_width - box_w) // 2
        box_y = screen_height - box_h - DIALOG_BOX_MARGIN_BOTTOM

        self._box_rect = pygame.Rect(box_x, box_y, box_w, box_h)

        pad = 16
        self._portrait_rect = pygame.Rect(
            box_x + pad, box_y + pad,
            DIALOG_PORTRAIT_SIZE, DIALOG_PORTRAIT_SIZE
        )
        text_x = box_x + pad + DIALOG_PORTRAIT_SIZE + pad
        self._name_pos = (text_x, box_y + pad)
        self._text_pos = (text_x, box_y + pad + 28)
        self._text_max_width = box_w - DIALOG_PORTRAIT_SIZE - pad * 3

        self._options_start_y = box_y + pad + 28 + 28 + 12
        self._hint_pos = (box_x + box_w - pad - 120, box_y + box_h - pad - 20)

        self._bg_surface = pygame.Surface((box_w, box_h), pygame.SRCALPHA)
        self._bg_surface.fill((20, 20, 30, DIALOG_BOX_ALPHA))

        self._font = None
        self._name_font = None
        self._hint_font = None
        self._init_fonts()

        self._portraits: dict = {}

    def _init_fonts(self):
        try:
            self._font = pygame.font.Font(None, 24)
            self._name_font = pygame.font.Font(None, 26)
            self._hint_font = pygame.font.Font(None, 18)
        except Exception:
            self._font = pygame.font.SysFont('microsoftyahei', 20)
            self._name_font = pygame.font.SysFont('microsoftyahei', 22, bold=True)
            self._hint_font = pygame.font.SysFont('microsoftyahei', 14)

    def _get_portrait(self, npc_id: str) -> Optional[pygame.Surface]:
        if npc_id in self._portraits:
            return self._portraits[npc_id]
        size = DIALOG_PORTRAIT_SIZE
        surface = pygame.Surface((size, size), pygame.SRCALPHA)
        colors = {'merchant': (70, 130, 180)}
        color = colors.get(npc_id, (100, 100, 100))
        pygame.draw.rect(surface, color, (4, 4, size - 8, size - 8), border_radius=8)
        cx, cy = size // 2, size // 3
        pygame.draw.circle(surface, (255, 220, 180), (cx, cy), 12)
        pygame.draw.rect(surface, color, (cx - 10, cy + 12, 20, 20))
        self._portraits[npc_id] = surface
        return surface

    def render(self, screen: pygame.Surface, npc_id: str, npc_name: str,
               displayed_text: str, is_text_complete: bool,
               responses: list, selected_option: int,
               is_choosing: bool, bubble_color: tuple = (70, 130, 180)):
        screen.blit(self._bg_surface, self._box_rect.topleft)
        portrait = self._get_portrait(npc_id)
        if portrait:
            screen.blit(portrait, self._portrait_rect.topleft)
        name_surface = self._name_font.render(npc_name, True, bubble_color)
        screen.blit(name_surface, self._name_pos)
        self._render_wrapped_text(screen, displayed_text, self._text_pos, self._text_max_width)
        if is_choosing and responses:
            self._render_options(screen, responses, selected_option)
        if is_choosing:
            hint = "↑↓选择  空格确认"
        elif is_text_complete:
            hint = "空格继续"
        else:
            hint = "空格跳过"
        hint_surface = self._hint_font.render(hint, True, (180, 180, 180))
        screen.blit(hint_surface, self._hint_pos)

    def _render_wrapped_text(self, screen: pygame.Surface, text: str,
                             pos: Tuple[int, int], max_width: int):
        x, y = pos
        line_height = 26
        current_line = ""
        for char in text:
            test_line = current_line + char
            test_surface = self._font.render(test_line, True, (255, 255, 255))
            if test_surface.get_width() > max_width:
                line_surface = self._font.render(current_line, True, (255, 255, 255))
                screen.blit(line_surface, (x, y))
                y += line_height
                current_line = char
            else:
                current_line = test_line
        if current_line:
            line_surface = self._font.render(current_line, True, (255, 255, 255))
            screen.blit(line_surface, (x, y))

    def _render_options(self, screen: pygame.Surface, responses: list, selected_option: int):
        y = self._options_start_y
        x = self._text_pos[0] + 16
        for i, resp in enumerate(responses):
            is_selected = (i == selected_option)
            prefix = "▶ " if is_selected else "  "
            color = (255, 220, 80) if is_selected else (200, 200, 200)
            text = f"{prefix}{resp['text']}"
            surface = self._font.render(text, True, color)
            screen.blit(surface, (x, y))
            y += 26 + DIALOG_OPTION_GAP
