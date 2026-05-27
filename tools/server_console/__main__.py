# -*- coding: utf-8 -*-
"""Server Console GUI 入口模块。

支持通过 python -m tools.server_console 启动 GUI 控制台。
"""

import logging
import sys

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s][%(levelname)s][%(name)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[logging.StreamHandler(sys.stdout)],
)

logger = logging.getLogger("server_console")


def main() -> None:
    """启动服务器控制台 GUI。"""
    logger.info("main, starting Server Console GUI")

    try:
        from .gui import ServerConsoleGUI

        app = ServerConsoleGUI()
        app.run()
    except ImportError as e:
        logger.error(f"main, import error, {e}")
        print(f"Error: {e}")
        print("Please install customtkinter: pip install customtkinter")
        sys.exit(1)
    except Exception as e:
        logger.error(f"main, unexpected error, {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
