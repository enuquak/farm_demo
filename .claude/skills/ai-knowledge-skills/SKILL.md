---
name: ai-knowledge-skills
description: Skill 体系架构、编写指南、标准章节模板、交叉引用规范。
metadata:
  type: reference
---

# AI Knowledge Skills 体系

## 概述

AI Knowledge Skills 是将项目设计知识从"需求文档"转化为"开发指南"的体系。每个 skill 聚焦一个子系统，供 AI 开发者按需加载。

## 架构设计

### 目录结构

```
.claude/skills/
├── server-architecture/SKILL.md
├── scene-system/SKILL.md
├── crop-system/SKILL.md
└── ...
```

### Frontmatter 规范

```yaml
---
name: skill-name
description: 一句话描述
metadata:
  type: reference
---
```

## 关键流程

### 创建新 Skill 的步骤

1. 确定子系统和源文档
2. 创建 `.claude/skills/{name}/` 目录
3. 创建 `SKILL.md` 文件
4. 按标准章节模板组织内容
5. 添加交叉引用
6. 验证格式和内容

### 内容来源

- `docs/superpowers/specs/` — 设计文档
- `docs/superpowers/plans/` — 实施计划
- `openspec/specs/` — 规范文档
- 实际代码

## 标准章节模板

```markdown
# 系统名称

## 概述
简要说明系统的定位和职责

## 架构设计
核心设计模式、数据结构、组件关系

## 关键流程
主要业务流程的步骤说明

## 关键代码路径
- 服务器：path/to/file.cpp/.h
- 客户端：path/to/file.py
- 配置：path/to/config

## 常见陷阱
从测试报告中提取的常见错误和解决方案

## 扩展指南
如何添加新功能的模板和步骤

## 相关 Skill
- [[other-skill]] — 关联说明
```

## 交叉引用规范

使用 `[[skill-name]]` 标记相关 skill：
- 指向存在的 skill
- 说明关联关系
- 不要创建不存在的引用

## 常见陷阱

### 内容与 spec 不一致

skill 内容过时或与设计文档矛盾：
- 从 spec 提炼而非复制
- 定期审查更新

### 交叉引用断裂

引用的 skill 不存在：
- 创建 skill 前检查依赖
- 使用 [[name]] 语法自动标记

### frontmatter 缺失

skill 文件没有 frontmatter：
- 必须包含 name、description、metadata.type

## 扩展指南

### 为新子系统创建 Skill

1. 确认子系统有完整的设计文档
2. 按标准模板创建 skill
3. 添加到相关 skill 的交叉引用中

## 相关 Skill

- [[server-architecture]] — 服务器架构（skill 示例）
