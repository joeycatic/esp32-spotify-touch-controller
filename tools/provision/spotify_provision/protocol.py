"""Validated, secret-safe serial provisioning protocol."""

from __future__ import annotations

import json
import os
import time
from dataclasses import dataclass
from typing import Any, Callable, Iterable, Sequence


class ProvisioningError(RuntimeError):
    pass


SERIAL_CHUNK_BYTES = 128
SERIAL_CHUNK_PAUSE_SECONDS = 0.02


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


def _device_group(port: str) -> str | None:
    """Name of the group that owns the port, when the platform can tell us."""
    try:
        import grp

        return grp.getgrgid(os.stat(port).st_gid).gr_name
    except (ImportError, KeyError, OSError):
        return None


def _session_groups() -> Sequence[str]:
    try:
        import grp

        return [grp.getgrgid(gid).gr_name for gid in os.getgroups()]
    except (ImportError, KeyError, OSError):
        return []


def _group_lists_current_user(group: str) -> bool:
    try:
        import getpass
        import grp

        return getpass.getuser() in grp.getgrnam(group).gr_mem
    except (ImportError, KeyError, OSError):
        return False


def _permission_message(
    port: str,
    group: str | None,
    session: Iterable[str],
    listed_in_group: Callable[[str], bool],
) -> str:
    lines = [f"No permission to open {port}."]
    if group is None:
        lines.append("Check that your user may read and write the device.")
        return "\n".join(lines)
    if group in session:
        lines.append(
            f"Your session already carries the {group!r} group, so something else is"
        )
        lines.append("denying access (a lock held by another program, or SELinux).")
        return "\n".join(lines)
    if listed_in_group(group):
        lines.append(
            f"You are a member of {group!r}, but this login session predates that."
        )
        lines.append("Log out and back in to pick it up, or run just this command with:")
        lines.append(f"  sg {group} -c 'make provision'")
        return "\n".join(lines)
    lines.append(f"The device is owned by the {group!r} group. Join it with:")
    lines.append(f"  sudo usermod -aG {group} \"$USER\"")
    lines.append("then log out and back in.")
    return "\n".join(lines)


def check_port_access(
    port: str,
    *,
    access: Callable[[str, int], bool] = os.access,
    device_group: Callable[[str], str | None] = _device_group,
    session_groups: Callable[[], Sequence[str]] = _session_groups,
    listed_in_group: Callable[[str], bool] = _group_lists_current_user,
) -> None:
    """Fail fast on an unusable port, before the Spotify authorization flow."""
    if not os.path.exists(port):
        raise ProvisioningError(f"Serial port {port} does not exist")
    if not access(port, os.R_OK | os.W_OK):
        raise ProvisioningError(
            _permission_message(
                port, device_group(port), session_groups(), listed_in_group
            )
        )


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

    try:
        connection_context = factory(port=port, baudrate=115200, timeout=0.5)
    except OSError as error:
        raise ProvisioningError(f"Could not open {port}: {error}") from error

    with connection_context as connection:
        if settle_seconds:
            time.sleep(settle_seconds)
        connection.reset_input_buffer()
        for offset in range(0, len(encoded), SERIAL_CHUNK_BYTES):
            chunk = encoded[offset : offset + SERIAL_CHUNK_BYTES]
            if connection.write(chunk) != len(chunk):
                raise ProvisioningError("Could not write the complete provisioning message")
            connection.flush()
            if offset + len(chunk) < len(encoded):
                time.sleep(SERIAL_CHUNK_PAUSE_SECONDS)

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
