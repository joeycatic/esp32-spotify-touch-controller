#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

CLI_VERSION="1.5.1"
mkdir -p "${PROJECT_ROOT}/.tools" "${PROJECT_ROOT}/.arduino/data" \
  "${PROJECT_ROOT}/.arduino/downloads" "${PROJECT_ROOT}/.arduino/user"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  machine="$(uname -m)"
  case "${machine}" in
    x86_64) archive_arch="Linux_64bit" ;;
    aarch64|arm64) archive_arch="Linux_ARM64" ;;
    *) echo "Unsupported CPU architecture: ${machine}" >&2; exit 1 ;;
  esac
  archive="${PROJECT_ROOT}/.tools/arduino-cli.tar.gz"
  curl --fail --location --silent --show-error \
    "https://github.com/arduino/arduino-cli/releases/download/v${CLI_VERSION}/arduino-cli_${CLI_VERSION}_${archive_arch}.tar.gz" \
    --output "${archive}"
  tar -xzf "${archive}" -C "${PROJECT_ROOT}/.tools" arduino-cli
  rm -f "${archive}"
fi

"${ARDUINO_CLI}" core update-index --config-file "${ARDUINO_CONFIG}"
"${ARDUINO_CLI}" core install esp32:esp32@3.3.11 --config-file "${ARDUINO_CONFIG}"
"${ARDUINO_CLI}" lib install \
  "lvgl@8.4.0" \
  "GFX Library for Arduino@1.5.0" \
  "ArduinoJson@7.4.3" \
  "TJpg_Decoder@1.1.0" \
  --config-file "${ARDUINO_CONFIG}"

python3 -m venv "${PROJECT_ROOT}/.venv"
"${PROJECT_ROOT}/.venv/bin/python" -m pip install --quiet --upgrade pip
"${PROJECT_ROOT}/.venv/bin/python" -m pip install --quiet -r "${PROJECT_ROOT}/tools/provision/requirements.txt"

echo "Toolchain ready: $("${ARDUINO_CLI}" version)"

