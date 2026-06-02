# scripts/client/test_npc_integration.py
"""NPC 对话系统集成测试。"""
import pygame
import sys
import os

# 添加项目根路径，以支持 scripts.client.* 包导入
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

from scripts.client.npc_sprite import NPCSprite
from scripts.client.npc_manager import NPCManager
from scripts.client.dialog_engine import DialogEngine, DialogState
from scripts.client.affection_system import AffectionSystem
from scripts.client.dialog_ui import DialogUI
from scripts.client.bubble_ui import BubbleUI


def test_npc_sprite():
    """测试 NPC 精灵创建和动画。"""
    sprite = NPCSprite('merchant', '商人老李', 0, 0, '', zoom=4)
    assert sprite.npc_id == 'merchant'
    assert sprite.name == '商人老李'
    assert sprite.rect.width == 128  # 32 * 4
    sprite.update_animation(0.1)
    print("  NPCSprite 测试通过")


def test_npc_manager():
    """测试 NPC 管理器。"""
    mgr = NPCManager(zoom=4)
    assert len(mgr._npcs) > 0

    mgr.set_current_scene('farm')
    farm_npcs = mgr.get_npcs_in_scene()
    print(f"  农场 NPC 数量: {len(farm_npcs)}")

    mgr.update_schedule('08:00')
    farm_npcs = mgr.get_npcs_in_scene()
    print(f"  08:00 农场 NPC: {[n.npc_id for n in farm_npcs]}")

    mgr.update_schedule('14:00')
    house_npcs = mgr.get_npcs_in_scene()
    print(f"  14:00 农场 NPC: {[n.npc_id for n in house_npcs]}")

    print("  NPCManager 测试通过")


def test_dialog_engine():
    """测试对话引擎。"""
    engine = DialogEngine()

    started = engine.start_dialog('merchant', affection=0, time_slot='08:00')
    assert started
    assert engine.state == DialogState.TYPING

    engine.advance()
    assert engine.state == DialogState.WAITING_INPUT

    engine.advance()
    assert engine.state == DialogState.TYPING

    engine.advance()
    engine.advance()
    assert engine.state == DialogState.CHOOSING
    assert len(engine.responses) > 0

    engine.select_option(1)
    engine.advance()

    while engine.is_active:
        engine.advance()

    print("  DialogEngine 测试通过")


def test_affection_system():
    """测试好感度系统。"""
    system = AffectionSystem()

    assert system.get_affection('merchant') == 0
    system.add_affection('merchant', 10)
    assert system.get_affection('merchant') == 10

    system.add_affection('merchant', 95)
    assert system.get_affection('merchant') == 100

    assert system.can_gift('merchant')
    system.record_gift('merchant')
    assert not system.can_gift('merchant')

    system.reset_daily_gifts()
    assert system.can_gift('merchant')

    print("  AffectionSystem 测试通过")


def test_dialog_ui():
    """测试对话框 UI。"""
    ui = DialogUI(800, 600)
    assert ui._box_rect.width > 0
    assert ui._box_rect.height > 0
    print("  DialogUI 测试通过")


def test_bubble_ui():
    """测试气泡 UI。"""
    ui = BubbleUI()

    ui.show_dots('merchant')
    assert 'merchant' in ui._active_bubbles

    ui.hide_dots('merchant')
    assert 'merchant' not in ui._active_bubbles

    ui.show_feedback('merchant', '❤️ +5', 1.0)
    assert 'merchant' in ui._active_bubbles

    ui.update(1.5)
    assert 'merchant' not in ui._active_bubbles

    print("  BubbleUI 测试通过")


if __name__ == '__main__':
    pygame.init()
    screen = pygame.display.set_mode((100, 100))

    test_npc_sprite()
    test_npc_manager()
    test_dialog_engine()
    test_affection_system()
    test_dialog_ui()
    test_bubble_ui()

    print("\n所有测试通过！")
    pygame.quit()
