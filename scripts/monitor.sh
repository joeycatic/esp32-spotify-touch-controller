#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_cli
serial_port="$(resolve_port)"

"${ARDUINO_CLI}" monitor \
  --config-file "${ARDUINO_CONFIG}" \
  --port "${serial_port}" \
  --config baudrate=115200

