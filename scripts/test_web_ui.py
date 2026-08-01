#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "playwright>=1.45",
#   "pyserial>=3.5",
# ]
# ///
"""Compatibility entry point for the modular Web UI regression suite.

Run with: uv run scripts/test_web_ui.py
"""

from web_ui_test.runner import main


if __name__ == "__main__":
    raise SystemExit(main())
