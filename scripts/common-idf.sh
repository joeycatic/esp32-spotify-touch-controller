#!/usr/bin/env bash
# Shared ESP-IDF environment for the source-built 7B target. The PowerShell
# scripts resolve tool paths by hand to avoid a Windows GDB probe that hangs;
# export.sh has no such problem on POSIX, so this uses it directly.
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

IDF_BUILD_DIR="${PROJECT_ROOT}/build/firmware-idf"
IDF_PROJECT_DIR="${PROJECT_ROOT}/firmware/idf"
ESPTOOL="${PROJECT_ROOT}/.arduino/data/packages/esp32/tools/esptool_py/5.3.1/esptool"

activate_idf() {
  export IDF_PATH="${IDF_PATH:-${PROJECT_ROOT}/.tools/esp-idf}"
  export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-${PROJECT_ROOT}/.tools/idf-tools}"

  if [[ ! -d "${IDF_PATH}" ]]; then
    echo "ESP-IDF is missing at ${IDF_PATH}. See firmware/idf/README.md." >&2
    exit 1
  fi

  local version
  version="$(git -C "${IDF_PATH}" describe --tags --exact-match 2>/dev/null || true)"
  if [[ "${version}" != "v5.5.5" ]]; then
    echo "This target requires ESP-IDF v5.5.5 (found: ${version:-unknown})." >&2
    exit 1
  fi

  # One 5.5 environment must exist and be unambiguous, matching build-idf.ps1.
  local envs=()
  while IFS= read -r candidate; do
    envs+=("${candidate}")
  done < <(find "${IDF_TOOLS_PATH}/python_env" -maxdepth 1 -type d \
    -name 'idf5.5_py*_env' 2>/dev/null | sort)
  if [[ ${#envs[@]} -ne 1 ]]; then
    echo "Install exactly one ESP-IDF 5.5 Python environment with" \
      "idf_tools.py install-python-env (found ${#envs[@]})." >&2
    exit 1
  fi
  export IDF_PYTHON_ENV_PATH="${envs[0]}"

  # export.sh is chatty and unsets -u internally; isolate it from our shell opts.
  set +u
  # shellcheck disable=SC1091
  source "${IDF_PATH}/export.sh" >/dev/null
  set -u
}

require_esptool() {
  if [[ ! -x "${ESPTOOL}" ]]; then
    echo "esptool is missing. Run: make bootstrap" >&2
    exit 1
  fi
}
