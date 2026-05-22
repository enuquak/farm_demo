#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os

def create_workspace_dir():
    # 要创建的文件夹名称
    dir_name = "agent_workspace_data"
    
    # 如果不存在则创建，存在则不报错
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)
        print(f"[OK] 成功创建文件夹：{dir_name}")
    else:
        print(f"[INFO] 文件夹已存在：{dir_name}")

if __name__ == "__main__":
    create_workspace_dir()