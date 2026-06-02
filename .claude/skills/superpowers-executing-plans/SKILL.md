---
name: superpowers-executing-plans
description: Execute a superpowers plan inline (no subagents). Works through tasks sequentially, updates checkboxes, commits, and merges to main on completion.
metadata:
  type: workflow
  version: "1.0"
---

Execute a superpowers plan inline without subagents.

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

3. **Execute tasks sequentially**

   For each task in the plan:

   a. **Announce the task**:
      ```
      Working on Task N: <task description>
      ```

   b. **Execute each step** in the task:
      - Create/modify files as specified
      - Follow the code exactly as written in the plan
      - Run build/compile commands if specified
      - Run tests if specified

   c. **After all steps in the task are done**:
      - Update the plan file: `- [ ]` → `- [x]` for all steps in this task
      - Commit all changes including the plan file update:
        ```bash
        git add -A
        git commit -m "<commit message from plan>"
        ```

   d. **Report task completion**:
      ```
      ✓ Task N complete
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

      Resume with: /superpowers:executing-plans <plan-file>
      ```

**Step Execution Rules**

For each step in a task:

1. **Code blocks** — Write the code exactly as specified in the plan
2. **Build commands** — Run them and verify success
3. **Test commands** — Run them and verify success
4. **Commit commands** — Use the exact commit message from the plan

If a step fails:
- Report the error
- Pause execution
- Don't skip to the next task

**Checkbox Update Format**

When updating the plan file, replace exactly:
- `- [ ]` → `- [x]` for completed steps

Do NOT modify any other content in the plan file.

**Guardrails**
- Always read the full plan file before starting
- Update checkboxes immediately after each task completes
- Commit after each task (don't batch)
- If a task fails, pause and report — don't skip
- Never modify code that isn't in the current task's scope
- Merge to main only when ALL tasks are complete
- If already on main, create a feature branch first
- Follow the plan's code exactly — don't improvise unless the plan says to
