#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os

def create_dir(path, description):
    if not os.path.exists(path):
        os.makedirs(path)
        print(f"[OK] 创建目录：{path}")
    else:
        print(f"[INFO] 目录已存在：{path}")

if __name__ == "__main__":
    # 智能体工作数据目录
    create_dir("agent_workspace_data", "智能体工作数据")

    # 预知识库目录
    create_dir("pre_knowledge", "预知识库根目录")
    create_dir("pre_knowledge/code", "开发智能体预知识")
    create_dir("pre_knowledge/test", "测试智能体预知识")

    # 临时文件目录（测试脚本、临时输出等）
    create_dir("tmp", "临时文件目录")
