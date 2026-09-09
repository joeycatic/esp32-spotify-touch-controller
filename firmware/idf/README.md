# Source-built 7B comparison firmware

This target builds the existing Spotify application with Arduino ESP32 3.3.11
as a component of ESP-IDF 5.5.5. It rebuilds the SDK and bootloader so the
Waveshare 7B cache and PSRAM settings actually affect the LCD driver. The normal
Arduino CLI build still uses its installed precompiled SDK.

This target now runs on the connected 7B with a working display. Idle LVGL
flush throughput measured 54 MB/s against 14 MB/s from the Arduino CLI build,
and time spent in flush fell from 83 ms to 22 ms per five-second window -- bus
time the panel's bounce-buffer refill ISR was previously being starved of.
Flicker under Spotify load is **not yet verified**, and several changes landed
together, so no single one is credited. See `docs/hardware-checklist.md`.
The application retains its 30 MHz pixel clock, ten-line bounce buffers, and
single framebuffer. The vendor memory profile selects 120 MHz octal PSRAM,
64-byte cache lines, a 64 KB data cache and 32 KB instruction cache,
instructions and read-only data in PSRAM, and performance optimization.

The first version of this target shipped only part of that. It set the cache
*line* size and left the cache *sizes* at ESP-IDF's defaults (32 KB data,
16 KB instruction), because `sdkconfig.defaults` never named them and nothing
checked -- so a build that reported itself as carrying the vendor profile was
running half the vendor's data cache, which is the cache the bounce-buffer
refill ISR depends on. `main/sketch.cpp` now asserts every setting in the
profile, so an incomplete one fails the build instead of shipping.

120 MHz octal PSRAM is experimental in ESP-IDF and is only stable while die
temperature is stable; Espressif documents random access failures once the chip
drifts about 20 C from where it powered on. Because this device runs for hours
and self-heats, `CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR` is
enabled so the timing point is re-measured at runtime. Waveshare's published
defaults set only the measurement interval, whose Kconfig `depends on` that
switch, so it never took effect for their short-running demo.

## Build on Windows

Install the normal project dependencies first. Use a recursive ESP-IDF
`v5.5.5` checkout at `.tools/esp-idf`, then install its ESP32-S3 tools and Python
environment using `tools/idf_tools.py`, with `IDF_TOOLS_PATH` pointing to
`.tools/idf-tools`. Creating the IDF Python environment requires a base Python
interpreter, not an already active virtual environment.

From the repository root:

```powershell
./scripts/build-idf.ps1
```

The script also accepts `-IdfPath` and `-IdfToolsPath`. It copies the pinned
Arduino sources into ignored `build/idf-components/arduino` and enables
the Arduino libraries needed by the app. LittleFS is pinned in the component
manifest/lockfile. LVGL, ArduinoJson, Arduino GFX, and TJpg_Decoder come from the
normal project's pinned `.arduino/user/libraries` installation.

Output is separate in `build/firmware-idf`. Generated SDK configuration is in
`build/idf-sdkconfig`; this is cached by IDF and takes precedence over
`sdkconfig.defaults`, so editing the defaults alone silently keeps the previous
profile. `scripts/build-idf.ps1` now deletes that cache whenever the defaults
are newer. Always confirm the result by grepping the generated file, not the
defaults you intended.

## Hardware comparison

The partition CSV matches the deployed 16 MB Arduino layout. Preserve NVS and
OTA metadata. Do not use a whole-flash erase or a vendor merged image for an
application update. Use generated flash offsets and verify written images.
Keep a locally verified full backup and recovery script before flashing.

For the connected, backed-up 7B selecting app0, the checked flash script is:

```powershell
./scripts/flash-idf.ps1 -Port COM4
```

It requires the locally saved full backup and matching manifest, checks the
partition bytes, and preserves NVS and OTA selection. Do not use it on another
device or after changing OTA slots without checking the selected application.

After flashing, disconnect **all** power for ten seconds and reconnect UART1.
Warm-reset black screens have occurred even with the unmodified vendor demo.
Check the PSRAM memory test and `[7B] source-built SDK` boot line, then compare
idle display, touch/scrolling, and Spotify networking/artwork. A clean boot log
does not establish a stable physical picture; record the user's observation.
