#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

cmake -S "${PROJECT_ROOT}/tests/native" -B "${PROJECT_ROOT}/build/tests"
cmake --build "${PROJECT_ROOT}/build/tests"
ctest --test-dir "${PROJECT_ROOT}/build/tests" --output-on-failure
python3 -m unittest discover -s "${PROJECT_ROOT}/tests/python" -v

