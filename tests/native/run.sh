#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "${PROJECT_ROOT}/build/tests"

g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  -I"${PROJECT_ROOT}/firmware/SpotifyController/src" \
  -I"${PROJECT_ROOT}/.arduino/user/libraries/ArduinoJson/src" \
  "${PROJECT_ROOT}/tests/native/test_core.cpp" \
  "${PROJECT_ROOT}/firmware/SpotifyController/src/core/AppState.cpp" \
  "${PROJECT_ROOT}/firmware/SpotifyController/src/core/RuntimePolicy.cpp" \
  "${PROJECT_ROOT}/firmware/SpotifyController/src/provision/ProvisioningValidation.cpp" \
  "${PROJECT_ROOT}/firmware/SpotifyController/src/spotify/SpotifyParser.cpp" \
  "${PROJECT_ROOT}/firmware/SpotifyController/src/spotify/SpotifyRequest.cpp" \
  -o "${PROJECT_ROOT}/build/tests/core_tests"

"${PROJECT_ROOT}/build/tests/core_tests"
