// Arduino-as-component supplies app_main and calls the original setup/loop.
#include "sdkconfig.h"
// Assert the whole memory profile, not part of it. An earlier build set the
// cache LINE size and reported itself as carrying "the vendor memory profile"
// while silently keeping ESP-IDF's default 32 KB data / 16 KB instruction cache,
// because sdkconfig.defaults never named the sizes and nothing checked.
static_assert(CONFIG_SPIRAM_SPEED == 120, "7B comparison requires 120 MHz PSRAM");
static_assert(CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE == 64, "RGB bounce buffers require 64-byte cache lines");
static_assert(CONFIG_ESP32S3_DATA_CACHE_SIZE == 0x10000, "RGB bounce buffers require the 64 KB data cache");
static_assert(CONFIG_ESP32S3_INSTRUCTION_CACHE_SIZE == 0x8000, "7B profile requires the 32 KB instruction cache");
#if !CONFIG_SPIRAM_FETCH_INSTRUCTIONS || !CONFIG_SPIRAM_RODATA
#error "7B comparison requires instruction and rodata placement in PSRAM"
#endif
// The inverse of the obvious assertion: temperature-tracked PSRAM retuning must
// stay OFF. It supports only flash vendor 0xC8/0x20 and aborts startup on this
// board's flash, boot-looping the device before app code runs. See the note in
// sdkconfig.defaults.
#if CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR
#error "PSRAM temperature retuning boot-loops this board; see sdkconfig.defaults"
#endif
#include "../../SpotifyController/SpotifyController.ino"
