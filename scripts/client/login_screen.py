"""
登录界面模块
使用 PyGame 实现登录界面，包含账号输入框和进入按钮
"""
import pygame
import sys
import os
from typing import Optional, Callable, Dict, Any

from .login_flow import LoginFlowManager, LoginState


class LoginScreen:
    """
    登录界面
    显示登录界面，处理用户输入，调用登录流程管理器
    """

    # 颜色定义
    COLOR_BG = (245, 245, 220)          # 米色背景
    COLOR_TITLE = (34, 139, 34)         # 绿色标题
    COLOR_INPUT_BG = (255, 255, 255)    # 白色输入框背景
    COLOR_INPUT_BORDER = (169, 169, 169)  # 灰色输入框边框
    COLOR_INPUT_ACTIVE = (0, 120, 215)  # 蓝色激活边框
    COLOR_BUTTON = (34, 139, 34)        # 绿色按钮
    COLOR_BUTTON_HOVER = (50, 205, 50)  # 亮绿色悬停
    COLOR_BUTTON_DISABLED = (169, 169, 169)  # 灰色禁用
    COLOR_TEXT = (51, 51, 51)           # 深灰色文字
    COLOR_TEXT_LIGHT = (128, 128, 128)  # 浅灰色文字
    COLOR_ERROR = (220, 20, 60)         # 红色错误
    COLOR_SUCCESS = (0, 128, 0)         # 绿色成功

    # 界面尺寸
    WINDOW_WIDTH = 800
    WINDOW_HEIGHT = 600

    # 输入框尺寸
    INPUT_WIDTH = 300
    INPUT_HEIGHT = 40
    INPUT_MAX_LENGTH = 32

    # 按钮尺寸
    BUTTON_WIDTH = 200
    BUTTON_HEIGHT = 45

    def __init__(self, host: str = '127.0.0.1', port: int = 8888):
        """
        初始化登录界面

        Args:
            host: Gate 服务器地址
            port: Gate 服务器端口
        """
        self.host = host
        self.port = port

        # PyGame 初始化
        pygame.init()
        self.screen = pygame.display.set_mode((self.WINDOW_WIDTH, self.WINDOW_HEIGHT))
        pygame.display.set_caption("Farm Demo - Login")

        # 字体
        self.font_title = pygame.font.Font(None, 72)
        self.font_input = pygame.font.Font(None, 28)
        self.font_button = pygame.font.Font(None, 32)
        self.font_error = pygame.font.Font(None, 24)
        self.font_hint = pygame.font.Font(None, 20)

        # 输入框状态
        self._input_text = ""
        self._input_active = False
        self._input_rect = pygame.Rect(
            (self.WINDOW_WIDTH - self.INPUT_WIDTH) // 2,
            280,
            self.INPUT_WIDTH,
            self.INPUT_HEIGHT
        )

        # 按钮状态
        self._button_rect = pygame.Rect(
            (self.WINDOW_WIDTH - self.BUTTON_WIDTH) // 2,
            350,
            self.BUTTON_WIDTH,
            self.BUTTON_HEIGHT
        )
        self._button_hover = False

        # 登录流程管理器
        self._login_flow = LoginFlowManager(host, port)
        self._login_flow.set_on_state_change(self._on_state_change)
        self._login_flow.set_on_success(self._on_login_success)
        self._login_flow.set_on_error(self._on_login_error)

        # 状态
        self._current_state = LoginState.IDLE
        self._status_message = ""
        self._error_message = ""
        self._is_connecting = False

        # 登录结果
        self._login_result: Optional[Dict[str, Any]] = None

        # 时钟
        self._clock = pygame.time.Clock()

    @property
    def login_result(self) -> Optional[Dict[str, Any]]:
        """获取登录结果"""
        return self._login_result

    def run(self) -> Optional[Dict[str, Any]]:
        """
        运行登录界面

        Returns:
            登录成功返回玩家数据字典，失败返回 None
        """
        running = True

        while running:
            # 处理事件
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                    break

                # 处理键盘事件
                if event.type == pygame.KEYDOWN:
                    self._handle_keydown(event)

                # 处理鼠标事件
                if event.type == pygame.MOUSEBUTTONDOWN:
                    self._handle_mouse_click(event.pos)
                if event.type == pygame.MOUSEMOTION:
                    self._handle_mouse_move(event.pos)

            # 处理登录流程消息
            self._login_flow.process_messages()

            # 检查登录成功
            if self._login_result:
                return self._login_result

            # 绘制界面
            self._draw()

            # 控制帧率
            self._clock.tick(60)

        # 清理
        self._login_flow.cancel_login()
        pygame.quit()
        return None

    def _handle_keydown(self, event: pygame.event.Event):
        """处理键盘按下事件"""
        # 如果正在连接中，忽略输入
        if self._is_connecting:
            return

        if event.key == pygame.K_RETURN:
            # 回车键触发登录
            self._start_login()
        elif event.key == pygame.K_BACKSPACE:
            # 退格键删除字符
            self._input_text = self._input_text[:-1]
        elif event.key == pygame.K_TAB:
            # Tab 键切换输入框激活状态
            self._input_active = not self._input_active
        elif event.unicode and len(self._input_text) < self.INPUT_MAX_LENGTH:
            # 输入字符
            if event.unicode.isprintable():
                self._input_text += event.unicode

    def _handle_mouse_click(self, pos: tuple):
        """处理鼠标点击事件"""
        # 如果正在连接中，忽略点击
        if self._is_connecting:
            return

        # 检查输入框点击
        if self._input_rect.collidepoint(pos):
            self._input_active = True
        else:
            self._input_active = False

        # 检查按钮点击
        if self._button_rect.collidepoint(pos):
            self._start_login()

    def _handle_mouse_move(self, pos: tuple):
        """处理鼠标移动事件"""
        self._button_hover = self._button_rect.collidepoint(pos)

    def _start_login(self):
        """开始登录流程"""
        # 清除错误消息
        self._error_message = ""

        # 验证输入
        if not self._input_text.strip():
            self._error_message = "请输入账号 ID"
            return

        # 设置连接中状态
        self._is_connecting = True
        self._status_message = "连接中..."

        # 开始登录流程
        self._login_flow.start_login(self._input_text.strip())

    def _on_state_change(self, state: LoginState, message: str):
        """状态变化回调"""
        self._current_state = state
        self._status_message = message

        if state == LoginState.ERROR:
            self._is_connecting = False
        elif state == LoginState.SUCCESS:
            self._is_connecting = False

    def _on_login_success(self, result: Dict[str, Any]):
        """登录成功回调"""
        self._login_result = result

    def _on_login_error(self, error_message: str):
        """登录错误回调"""
        self._error_message = error_message
        self._is_connecting = False

    def _draw(self):
        """绘制界面"""
        # 清屏
        self.screen.fill(self.COLOR_BG)

        # 绘制标题
        title_text = self.font_title.render("Farm Demo", True, self.COLOR_TITLE)
        title_rect = title_text.get_rect(center=(self.WINDOW_WIDTH // 2, 120))
        self.screen.blit(title_text, title_rect)

        # 绘制副标题
        subtitle_text = self.font_error.render("Multiplayer Farm Game", True, self.COLOR_TEXT_LIGHT)
        subtitle_rect = subtitle_text.get_rect(center=(self.WINDOW_WIDTH // 2, 170))
        self.screen.blit(subtitle_text, subtitle_rect)

        # 绘制输入框标签
        label_text = self.font_input.render("Account ID:", True, self.COLOR_TEXT)
        label_rect = label_text.get_rect(midright=(self._input_rect.left - 10, self._input_rect.centery))
        self.screen.blit(label_text, label_rect)

        # 绘制输入框
        input_border_color = self.COLOR_INPUT_ACTIVE if self._input_active else self.COLOR_INPUT_BORDER
        pygame.draw.rect(self.screen, self.COLOR_INPUT_BG, self._input_rect)
        pygame.draw.rect(self.screen, input_border_color, self._input_rect, 2)

        # 绘制输入框文字
        if self._input_text:
            input_text_surface = self.font_input.render(self._input_text, True, self.COLOR_TEXT)
        else:
            input_text_surface = self.font_input.render("Enter your account ID...", True, self.COLOR_TEXT_LIGHT)

        # 裁剪文字到输入框范围内
        text_rect = input_text_surface.get_rect(midleft=(self._input_rect.left + 10, self._input_rect.centery))
        if text_rect.width > self._input_rect.width - 20:
            text_rect.width = self._input_rect.width - 20
        self.screen.blit(input_text_surface, text_rect, (0, 0, text_rect.width, text_rect.height))

        # 绘制光标（如果输入框激活）
        if self._input_active and not self._is_connecting:
            cursor_x = self._input_rect.left + 10 + min(
                self.font_input.size(self._input_text)[0],
                self._input_rect.width - 20
            )
            pygame.draw.line(
                self.screen, self.COLOR_TEXT,
                (cursor_x, self._input_rect.top + 8),
                (cursor_x, self._input_rect.bottom - 8),
                2
            )

        # 绘制按钮
        if self._is_connecting:
            button_color = self.COLOR_BUTTON_DISABLED
            button_text = "Connecting..."
        elif self._button_hover:
            button_color = self.COLOR_BUTTON_HOVER
            button_text = "Enter Game"
        else:
            button_color = self.COLOR_BUTTON
            button_text = "Enter Game"

        pygame.draw.rect(self.screen, button_color, self._button_rect, border_radius=8)
        pygame.draw.rect(self.screen, (0, 0, 0), self._button_rect, 2, border_radius=8)

        button_text_surface = self.font_button.render(button_text, True, (255, 255, 255))
        button_text_rect = button_text_surface.get_rect(center=self._button_rect.center)
        self.screen.blit(button_text_surface, button_text_rect)

        # 绘制状态消息
        if self._status_message:
            status_color = self.COLOR_TEXT
            if self._current_state == LoginState.CONNECTING:
                status_color = self.COLOR_TEXT_LIGHT
            elif self._current_state == LoginState.LOGGING_IN:
                status_color = self.COLOR_TEXT_LIGHT
            elif self._current_state == LoginState.SUCCESS:
                status_color = self.COLOR_SUCCESS

            status_text = self.font_error.render(self._status_message, True, status_color)
            status_rect = status_text.get_rect(center=(self.WINDOW_WIDTH // 2, 420))
            self.screen.blit(status_text, status_rect)

        # 绘制错误消息
        if self._error_message:
            error_text = self.font_error.render(self._error_message, True, self.COLOR_ERROR)
            error_rect = error_text.get_rect(center=(self.WINDOW_WIDTH // 2, 460))
            self.screen.blit(error_text, error_rect)

        # 绘制提示信息
        hint_text = self.font_hint.render("Press Enter or click button to start", True, self.COLOR_TEXT_LIGHT)
        hint_rect = hint_text.get_rect(center=(self.WINDOW_WIDTH // 2, 520))
        self.screen.blit(hint_text, hint_rect)

        # 更新显示
        pygame.display.flip()
