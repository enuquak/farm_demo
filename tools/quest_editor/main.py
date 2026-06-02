"""Entry point for the quest editor application."""

import os
import sys

# Ensure the tools/quest_editor directory is on the import path so that
# sibling modules (quest_node, quest_data, editor_app) can be imported
# without requiring a package install.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from editor_app import main as run_editor  # noqa: E402

if __name__ == "__main__":
    run_editor()
