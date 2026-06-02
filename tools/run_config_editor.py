#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""配置编辑器启动脚本。"""
import os
import sys

# 确保项目根目录在 Python 路径中
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, project_root)

from tools.config_editor.app import ConfigEditorApp


def main():
    app = ConfigEditorApp(project_root)
    app.run()


if __name__ == "__main__":
    main()
