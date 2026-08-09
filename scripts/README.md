# Automation Scripts

The directory is intentionally grouped by operational scope:

| Directory | Contents |
| --- | --- |
| `device/` | Flash, erase, desktop stream and WiFi recovery checks against hardware |
| `hooks/` | Claude Code UDP hook, serial bridge and cross-platform installer |
| `data/` | Timetable import and media/data acquisition utilities |
| `testing/` | Offline tests, browser regression, OTA E2E and their shared fixtures |
| `tools/` | Manual UDP/display preview utilities |

Use the layered test entry point for routine validation:

```bash
uv run --with pytest scripts/testing/test_suite.py unit
uv run scripts/testing/test_suite.py web
uv run scripts/testing/test_suite.py ota --device-url http://<device-ip>/ \
  --advertise-host <host-ip> --serial-port <serial-port>
```

`unit` is deterministic and does not require Playwright or hardware. `web`
uses the local firmware stub by default. `ota` is explicit because it flashes
the connected ESP32 and restores production firmware in its cleanup path.
