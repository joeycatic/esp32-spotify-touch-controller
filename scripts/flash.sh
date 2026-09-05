#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_cli
serial_port="$(resolve_port)"

"${PROJECT_ROOT}/scripts/build.sh"
"${ARDUINO_CLI}" upload \
  --config-file "${ARDUINO_CONFIG}" \
  --fqbn "${FQBN}" \
  --port "${serial_port}" \
  --input-dir "${PROJECT_ROOT}/build/firmware" \
  "${SKETCH_DIR}"

