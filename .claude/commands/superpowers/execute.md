---
name: "Superpowers: Execute Plan"
description: Execute a superpowers plan with subagent-driven development
category: Workflow
tags: [superpowers, plan, execution]
---

Execute a superpowers plan using subagent-driven development.

**Input**: Optionally specify a plan file path (e.g., `/superpowers:execute docs/superpowers/plans/2026-06-03-quest-ui.md`). If omitted, list available plans.

**Steps**

1. **Select the plan file**

   If a path is provided, use it. Otherwise:
   - List all `.md` files in `docs/superpowers/plans/`
   - Show available plans with their task counts
   - Use **AskUserQuestion tool** to let the user select

2. **Invoke the skill**

   Use the Skill tool to invoke `superpowers:subagent-driven-development` with the selected plan file.

3. **Report results**

   Show the execution results as reported by the skill.
