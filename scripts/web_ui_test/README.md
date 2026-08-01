# Web UI regression structure

The public command remains:

```bash
uv run scripts/test_web_ui.py
```

Responsibilities are separated as follows:

- `runner.py`: CLI parsing, suite orchestration, and cleanup.
- `firmware_stub.py`: HTTP behavior that mirrors firmware endpoints.
- `transports.py`: serial capture and live-device request throttling.
- `fixtures.py`: stable test data only.
- `device.py`: live-device snapshots, assertions, and restoration.
- `cases/`: feature-level browser cases. A new feature should normally add or
  update only its corresponding case module.

Case modules contain browser actions and assertions. They must not start
servers, launch browsers, parse CLI arguments, or own device restoration.
