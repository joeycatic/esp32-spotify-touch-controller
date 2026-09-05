#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

"${PROJECT_ROOT}/tests/native/run.sh"
python3 -m unittest discover -s "${PROJECT_ROOT}/tests/python" -v
