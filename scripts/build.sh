#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_cli

mkdir -p "${PROJECT_ROOT}/build/firmware"
cp "${SKETCH_DIR}/lv_conf.h" "${PROJECT_ROOT}/.arduino/user/libraries/lv_conf.h"
"${ARDUINO_CLI}" compile \
  --config-file "${ARDUINO_CONFIG}" \
  --fqbn "${FQBN}" \
  --warnings all \
  --build-path "${PROJECT_ROOT}/build/firmware" \
  "${SKETCH_DIR}"
