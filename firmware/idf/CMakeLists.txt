cmake_minimum_required(VERSION 3.24)

# Build the SDK as well as the sketch: Arduino CLI's precompiled qio_opi
# libraries cannot pick up sdkconfig changes made in application headers.
get_filename_component(SPOTCTL_ROOT "../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${SPOTCTL_ROOT}/build/idf-components/arduino")
set(SDKCONFIG "${SPOTCTL_ROOT}/build/idf-sdkconfig")
set(IDF_TARGET esp32s3)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(spotify_7b)

idf_component_get_property(arduino_lib arduino COMPONENT_LIB)
target_compile_definitions(${arduino_lib} PUBLIC
    BOARD_HAS_PSRAM ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1)
