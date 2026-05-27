"""
Farm Demo 客户端主入口
初始化 PyGame，运行登录界面，处理登录成功后的游戏状态
"""
import pygame
import sys
import os
import logging

# 添加项目根目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from scripts.client.login_screen import LoginScreen
from scripts.client.game_scene import GameScene
from scripts.client.log_init import init_logging, shutdown_logging


def main():
    """主函数"""
    # 初始化日志
    logger = init_logging(
        process_name="client",
        log_dir="./runtimeData/logs/client",
        level="info"
    )
    logger.info("[Client]Starting Farm Demo client")

    # 创建并运行登录界面
    login_screen = LoginScreen(host='127.0.0.1', port=8888)
    result = login_screen.run()

    if result:
        logger.info(f"[Client]Login successful: account_id={result['account_id']}, "
                    f"player_id={result['player_id']}, role_name={result['role_name']}")

        # 登录成功，打印玩家信息
        print("\n" + "=" * 50)
        print("Login Successful!")
        print("=" * 50)
        print(f"Account ID: {result['account_id']}")
        print(f"Player ID: {result['player_id']}")
        print(f"Role Name: {result['role_name']}")
        print(f"Server ID: {result['server_id']}")
        print(f"Level: {result['level']}")
        print(f"Position: ({result['pos_x']}, {result['pos_y']}, {result['pos_z']})")
        print(f"Scene: {result['scene_id']}")
        print("=" * 50)

        # 获取连接对象
        connection = result.get('connection')
        if connection:
            logger.info(f"[Client]Connection maintained for further game operations")

        # 进入游戏场景
        print("\nEntering game scene...")
        print("Controls: WASD or Arrow Keys to move, ESC to quit")
        try:
            game_scene = GameScene(connection, result)
            game_scene.run()
        except Exception as e:
            logger.error(f"[Client]Game scene error: {e}")
            print(f"Game error: {e}")

        # 断开连接
        if connection:
            connection.disconnect()
            logger.info("[Client]Connection disconnected")

    else:
        logger.info("[Client]Login cancelled or failed")
        print("Login cancelled.")

    # 关闭日志
    shutdown_logging()
    logger.info("[Client]Client exited")


if __name__ == "__main__":
    main()
