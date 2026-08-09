# Web UI regression structure

The public command remains:

```bash
uv run scripts/testing/test_web_ui.py
```

The layered suite has one discovery entry point:

```bash
# Fast, deterministic checks
uv run --with pytest scripts/testing/test_suite.py unit

# Browser regression against the firmware stub
uv run scripts/testing/test_suite.py web

# Full device OTA flow (requires a connected ESP32 and reachable host)
uv run scripts/testing/test_suite.py ota --device-url http://<device-ip>/ \
  --advertise-host <host-ip> --serial-port /dev/cu.usbmodem...
```

The offline tests intentionally do not require Playwright, a browser, WiFi, or
hardware. Web and hardware suites stay opt-in because they mutate device state
or require external services. `all` runs offline and Web UI suites; OTA is
explicit-only so a local command cannot accidentally flash a device.

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
