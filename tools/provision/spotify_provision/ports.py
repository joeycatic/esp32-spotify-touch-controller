"""USB serial discovery shared by flash, monitor, and provisioning."""

from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from typing import Iterable, Protocol


ESPRESSIF_VID = 0x303A
WCH_VID = 0x1A86
_CH343_DESCRIPTION = re.compile(r"CH343|USB Single Serial|USB[- ]Serial", re.I)


class PortLike(Protocol):
    device: str
    vid: int | None
    pid: int | None
    description: str | None
    manufacturer: str | None
    product: str | None


class PortResolutionError(RuntimeError):
    pass


@dataclass(frozen=True)
class Candidate:
    device: str
    kind: str
    priority: int
    description: str


def _candidate(port: PortLike) -> Candidate | None:
    description = " ".join(
        value for value in (port.description, port.manufacturer, port.product) if value
    )
    if port.vid == WCH_VID and (
        port.pid in {0x55D3, 0x55D4} or _CH343_DESCRIPTION.search(description)
    ):
        return Candidate(port.device, "7B UART1/CH343", 300, description)
    if port.vid == ESPRESSIF_VID:
        return Candidate(port.device, "Espressif native USB", 200, description)
    return None


def resolve_port(ports: Iterable[PortLike], explicit: str | None = None) -> str:
    """Resolve one safe port; never select an unrelated or ambiguous device."""
    if explicit:
        return explicit
    candidates = [candidate for port in ports if (candidate := _candidate(port))]
    if not candidates:
        raise PortResolutionError(
            "No supported ESP32 serial device found.\n"
            "7B: connect the UART1 / USB TO UART Type-C port with a data cable "
            "and route its DIP switch to ESP32.\n"
            "7B regular-USB fallback: hold BOOT while reconnecting USB.\n"
            "2-inch: connect its regular USB data port.\n"
            "On macOS, confirm CH343 or Espressif appears in System Information > USB; "
            "install/update the Waveshare WCH driver if CH343 appears there without a "
            "serial port. Override detection with PORT=/dev/..."
        )
    best_priority = max(candidate.priority for candidate in candidates)
    best = [candidate for candidate in candidates if candidate.priority == best_priority]
    if len(best) == 1:
        return best[0].device
    details = ", ".join(f"{item.device} ({item.kind})" for item in best)
    raise PortResolutionError(
        f"Multiple supported serial devices found: {details}. Set PORT=/dev/..."
    )


def detected_port(explicit: str | None = None) -> str:
    from serial.tools import list_ports

    return resolve_port(list_ports.comports(), explicit)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Resolve the controller serial port")
    parser.add_argument("--port", default=os.environ.get("PORT"))
    args = parser.parse_args(argv)
    try:
        print(detected_port(args.port))
        return 0
    except PortResolutionError as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
