---
name: superpowers-subagent-driven-development
description: Execute a superpowers plan using subagent-driven development. Dispatches subagents per task, updates checkboxes, commits, and merges to main on completion.
metadata:
  type: workflow
  version: "1.0"
---

Execute a superpowers plan using subagent-driven development.

**Input**: A plan file path from `docs/superpowers/plans/`. If not specified, look for plan files in the conversation context or prompt the user.

**Workflow**

1. **Read the plan file**

   Read the entire plan file to understand:
   - Total tasks and steps
   - Which steps are already completed (`- [x]`)
   - Which steps are pending (`- [ ]`)
   - File dependencies between tasks

2. **Create a working branch** (if not already on one)

   ```bash
   git checkout -b feat/<plan-name>
   ```

   Where `<plan-name>` is derived from the plan file name (e.g., `2026-06-03-quest-ui` → `feat/quest-ui`).

3. **Execute tasks using subagents**

   For each task in the plan:

   a. **Dispatch a subagent** with the task details:
      - Task description and all steps
      - File paths to create/modify
      - Code snippets from the plan
      - Instructions to commit after completion

   b. **Wait for subagent completion**

   c. **Update the plan file** — mark all steps in the task as completed:
      ```
      - [ ] → - [x]
      ```

   d. **Commit the plan file update**:
      ```bash
      git add docs/superpowers/plans/<plan-file>
      git commit -m "chore: mark task N complete in <plan-name>"
      ```

   e. **Continue to next task**

4. **On all tasks complete — merge to main**

   After all tasks are completed:

   a. **Ensure all changes are committed**:
      ```bash
      git add -A
      git status --short
      ```

   b. **Switch to main and merge**:
      ```bash
      git checkout main
      git merge feat/<plan-name> --no-ff -m "feat: complete <plan-name>"
      ```

   c. **Clean up the feature branch**:
      ```bash
      git branch -d feat/<plan-name>
      ```

   d. **Report completion**:
      ```
      ## ✅ Plan Complete: <plan-name>

      All N tasks completed and merged into main.
      Branch: feat/<plan-name> → main
      ```

5. **On partial completion (blocked/paused)**

   If execution stops early:

   a. **Commit all completed work**
   b. **Report status**:
      ```
      ## ⏸️ Plan Paused: <plan-name>

      Completed: M/N tasks
      Remaining: N-M tasks
      Branch: feat/<plan-name> (not merged)

      Resume with: /superpowers:subagent-driven-development <plan-file>
      ```

**Subagent Dispatch Template**

For each task, dispatch with:

```
Execute Task N from plan: <plan-file-path>

## Task Details
<task description and steps>

## Files to Modify
<file list from plan>

## Code
<code snippets from plan>

## Instructions
1. Create/modify the files as specified
2. Follow the code exactly as written in the plan
3. After completing all steps, commit with the specified commit message
4. Do NOT modify the plan file itself — I will update it
```

**Guardrails**
- Always read the full plan file before starting
- Update checkboxes immediately after each task completes
- Commit after each task (don't batch)
- If a task fails, pause and report — don't skip
- Never modify code that isn't in the current task's scope
- Merge to main only when ALL tasks are complete
- If already on main, create a feature branch first
