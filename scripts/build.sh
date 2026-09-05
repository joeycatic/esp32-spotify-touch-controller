#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_cli

mkdir -p "${PROJECT_ROOT}/build/firmware"
"${ARDUINO_CLI}" compile \
  --config-file "${ARDUINO_CONFIG}" \
  --fqbn "${FQBN}" \
  --warnings all \
  --build-path "${PROJECT_ROOT}/build/firmware" \
  "${SKETCH_DIR}"

