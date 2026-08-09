#!/usr/bin/env python3
"""Single entry point for the layered automation suites.

Examples:
    uv run --with pytest scripts/testing/test_suite.py unit
    uv run scripts/testing/test_suite.py web
    uv run scripts/testing/test_suite.py ota --device-url ... --advertise-host ... --serial-port ...
"""

from test_support.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
