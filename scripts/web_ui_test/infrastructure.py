"""Compatibility exports for Web UI test infrastructure."""

from .firmware_stub import FirmwareStubHandler
from .transports import DeviceRequestThrottle, SerialLogCapture

__all__ = [
    "DeviceRequestThrottle",
    "FirmwareStubHandler",
    "SerialLogCapture",
]
