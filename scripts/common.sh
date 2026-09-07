#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${PROJECT_ROOT}/.tools/arduino-cli"
ARDUINO_CONFIG="${PROJECT_ROOT}/arduino-cli.yaml"
SKETCH_DIR="${PROJECT_ROOT}/firmware/SpotifyController"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc"

require_cli() {
  if [[ ! -x "${ARDUINO_CLI}" ]]; then
    echo "Arduino CLI is missing. Run: make bootstrap" >&2
    exit 1
  fi
}

resolve_port() {
  local python="${PROJECT_ROOT}/.venv/bin/python"
  if [[ ! -x "${python}" ]]; then
    echo "Python environment is missing. Run: make bootstrap" >&2
    exit 1
  fi
  PYTHONPATH="${PROJECT_ROOT}/tools/provision" PORT="${PORT:-}" \
    "${python}" -m spotify_provision.ports
}
