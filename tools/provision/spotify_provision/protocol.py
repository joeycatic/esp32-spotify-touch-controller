"""Validated, secret-safe serial provisioning protocol."""

from __future__ import annotations

import json
import time
from dataclasses import dataclass
from typing import Any, Callable


class ProvisioningError(RuntimeError):
    pass


@dataclass(frozen=True)
class ProvisioningData:
    wifi_ssid: str
    wifi_password: str
    client_id: str
    refresh_token: str


def _validate(data: ProvisioningData) -> None:
    ssid_bytes = data.wifi_ssid.encode("utf-8")
    password_bytes = data.wifi_password.encode("utf-8")
    if not 1 <= len(ssid_bytes) <= 32:
        raise ProvisioningError("Wi-Fi SSID must contain 1 to 32 bytes")
    if password_bytes and not 8 <= len(password_bytes) <= 63:
        raise ProvisioningError("Wi-Fi password must contain 8 to 63 bytes")
    if not 16 <= len(data.client_id) <= 128:
        raise ProvisioningError("Spotify Client ID has an invalid length")
    if not 1 <= len(data.refresh_token) <= 2048:
        raise ProvisioningError("Spotify refresh token has an invalid length")
    if any(character in data.client_id for character in "\r\n"):
        raise ProvisioningError("Spotify Client ID contains invalid characters")
    if any(character in data.refresh_token for character in "\r\n"):
        raise ProvisioningError("Spotify refresh token contains invalid characters")


def encode_provisioning_message(data: ProvisioningData) -> bytes:
    _validate(data)
    payload = {
        "v": 1,
        "type": "provision",
        "wifi": {"ssid": data.wifi_ssid, "password": data.wifi_password},
        "spotify": {
            "client_id": data.client_id,
            "refresh_token": data.refresh_token,
        },
    }
    return (json.dumps(payload, separators=(",", ":"), ensure_ascii=False) + "\n").encode(
        "utf-8"
    )


def redacted_summary(data: ProvisioningData) -> str:
    return f"Wi-Fi: {data.wifi_ssid}; Spotify Client ID: [redacted]; token: [redacted]"


def _default_serial_factory(**kwargs: Any):
    import serial

    return serial.Serial(**kwargs)


def provision_serial(
    port: str,
    data: ProvisioningData,
    *,
    serial_factory: Callable[..., Any] | None = None,
    timeout_seconds: float = 10.0,
    settle_seconds: float = 2.0,
) -> bool:
    factory = serial_factory or _default_serial_factory
    encoded = encode_provisioning_message(data)
    deadline = time.monotonic() + timeout_seconds

    with factory(port=port, baudrate=115200, timeout=0.5) as connection:
        if settle_seconds:
            time.sleep(settle_seconds)
        connection.reset_input_buffer()
        connection.write(encoded)
        connection.flush()

        while time.monotonic() < deadline:
            line = connection.readline()
            if not line:
                continue
            try:
                response = json.loads(line.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                continue
            if response.get("v") != 1 or response.get("type") != "provision_result":
                continue
            if response.get("ok") is True:
                return True
            message = str(response.get("message") or response.get("code") or "rejected")
            raise ProvisioningError(f"The ESP32 rejected provisioning: {message}")

    raise ProvisioningError("Timed out waiting for the ESP32 provisioning response")

