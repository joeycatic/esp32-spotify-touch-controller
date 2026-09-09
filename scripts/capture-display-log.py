"""Capture display/boot diagnostics without printing provisioning or API data."""
import argparse
import time

import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", default="COM4")
parser.add_argument("--seconds", type=float, default=45)
args = parser.parse_args()

# Set modem lines before opening so this reader does not request a reset.
port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
port.dtr = False
port.rts = False
port.port = args.port
port.open()
deadline = time.monotonic() + args.seconds
markers = (
    "[7B]", "[board]", "esp_psram:", "octal_psram:", "MSPI Timing:",
    "Guru Meditation", "panic'ed", "assert failed", "Brownout detector",
    "Backtrace:", "rst:", "boot:", "esp_image:", "boot.esp32s3:",
    "Project name:", "App version:", "ESP-IDF:", "[recovery]",
)
try:
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if line and any(marker in line for marker in markers):
            print(line, flush=True)
finally:
    port.close()
