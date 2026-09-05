#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

if [[ ! -x "${PROJECT_ROOT}/.venv/bin/python" ]]; then
  echo "Python environment is missing. Run: make bootstrap" >&2
  exit 1
fi

PYTHONPATH="${PROJECT_ROOT}/tools/provision" \
  "${PROJECT_ROOT}/.venv/bin/python" -m spotify_provision "$@"

