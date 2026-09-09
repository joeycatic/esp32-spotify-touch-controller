get_filename_component(SPOTCTL_ROOT "../../.." ABSOLUTE)
set(sketch "${SPOTCTL_ROOT}/firmware/SpotifyController")
set(libraries "${SPOTCTL_ROOT}/.arduino/user/libraries")
file(GLOB_RECURSE app_sources "${sketch}/src/*.cpp")
file(GLOB_RECURSE gfx_sources
    "${libraries}/GFX_Library_for_Arduino/src/*.cpp"
    "${libraries}/GFX_Library_for_Arduino/src/*.c")
file(GLOB_RECURSE lvgl_sources "${libraries}/lvgl/src/*.c")
file(GLOB jpeg_sources
    "${libraries}/TJpg_Decoder/src/*.cpp" "${libraries}/TJpg_Decoder/src/*.c")
idf_component_register(
    SRCS "sketch.cpp" ${app_sources} ${gfx_sources} ${lvgl_sources} ${jpeg_sources}
    INCLUDE_DIRS "${sketch}" "${libraries}/lvgl" "${libraries}/lvgl/src"
        "${libraries}/ArduinoJson/src" "${libraries}/GFX_Library_for_Arduino/src"
        "${libraries}/TJpg_Decoder/src"
    REQUIRES arduino esp_lcd esp_psram esp_netif esp_wifi lwip)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE SPOTCTL_VENDOR_MEMORY_PROFILE=1)
# The application uses its existing lv_conf.h, not LVGL's IDF Kconfig profile.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -Wno-error=overloaded-virtual)
