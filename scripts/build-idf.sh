#!/usr/bin/env bash
# POSIX equivalent of scripts/build-idf.ps1.
set -euo pipefail

source "$(dirname "$0")/common-idf.sh"
activate_idf

# Use the already pinned Arduino installation without changing its files. Its
# broad component manifest pulls unrelated Matter/Zigbee dependencies; the
# project enables the libraries this application needs directly.
arduino_source="${PROJECT_ROOT}/.arduino/data/packages/esp32/hardware/esp32/3.3.11"
arduino_component="${PROJECT_ROOT}/build/idf-components/arduino"
if [[ ! -f "${arduino_source}/CMakeLists.txt" ]]; then
  echo "Run: make bootstrap (Arduino ESP32 3.3.11 and pinned libraries)." >&2
  exit 1
fi
mkdir -p "${arduino_component}"
for entry in cores libraries variants CMakeLists.txt Kconfig.projbuild; do
  rm -rf "${arduino_component:?}/${entry}"
  cp -r "${arduino_source}/${entry}" "${arduino_component}/"
done

# ESP-IDF caches the generated sdkconfig and it takes precedence over
# sdkconfig.defaults, so editing the defaults alone silently keeps the previous
# memory profile. Drop the cache whenever the defaults are newer than it; the
# static_asserts in main/sketch.cpp are the backstop if this ever misses.
sdk_defaults="${IDF_PROJECT_DIR}/sdkconfig.defaults"
sdk_cache="${PROJECT_ROOT}/build/idf-sdkconfig"
if [[ -f "${sdk_cache}" && "${sdk_defaults}" -nt "${sdk_cache}" ]]; then
  echo "sdkconfig.defaults changed; regenerating the cached sdkconfig."
  rm -f "${sdk_cache}"
fi

idf.py -C "${IDF_PROJECT_DIR}" -B "${IDF_BUILD_DIR}" build

# The defaults are not the built configuration. Confirm the vendor memory
# profile from the generated file, which is what actually reached the compiler.
echo
echo "Vendor memory profile in the generated sdkconfig:"
grep -E '^CONFIG_(ESP32S3_(DATA|INSTRUCTION)_CACHE_(LINE_)?[0-9]+(KB|B)|SPIRAM_(SPEED|MODE_OCT|FETCH_INSTRUCTIONS|RODATA))' \
  "${sdk_cache}" | sed 's/^/  /'
