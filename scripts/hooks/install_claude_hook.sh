#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if command -v uv >/dev/null 2>&1; then
  exec uv run --script "$SCRIPT_DIR/install_claude_hook.py" "$@"
elif command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN=python
else
  echo "Python 3 is required." >&2
  exit 1
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/install_claude_hook.py" "$@"
