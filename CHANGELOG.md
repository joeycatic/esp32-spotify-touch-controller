# Changelog

All notable changes to this project are documented here.

## [Unreleased]

## [1.0.0-rc.3] - 2026-09-09

Third release candidate. The 7B display is running on hardware for the first
time with measured PSRAM-bandwidth headroom; flicker under Spotify load is not
yet verified.

### Added

- Separate, experimental source-built ESP-IDF 5.5.5 / Arduino 3.3.11 target for testing the vendor 7B memory/cache profile without changing panel timings. Physical validation is pending.
- One universal ESP32-S3 firmware image for the 2-inch ST7789/CST816 board and the 7B 1024×600 RGB/GT911 board.
- Conservative I²C hardware detection, verified remembered profiles, recoverable headless provisioning, and profile-specific serial routing.
- Native 7B focused-split player, persistent bottom navigation, 64-pixel controls, wide library/device/QR views, and on-screen hold-to-reset.
- Shared USB serial resolver that recognizes Espressif native USB and the 7B CH343 UART adapter while ignoring macOS pseudo-ports.
- Profile-driven exact artwork sizes and wide-only bounded row thumbnails.

### Fixed

- The 7B keeps LVGL's draw buffer in internal RAM. `Board::allocateDrawBuffer`
  asked for PSRAM first and only fell back to internal, directly under a comment
  explaining why the render target must not live in PSRAM, so every boot took the
  slow path and flushes ran at the measured 7.5 MB/s instead of 15 MB/s. The 7B
  profile also asked for 40 rows x 2 buffers (160 KiB), which cannot fit internal
  RAM beside the panel and so guaranteed that fallback; it is back to the intended
  16 rows x 1 buffer.
- The 7B power-cycles the panel rail through the I/O expander before releasing
  LCD reset. The expander has its own power and latches its outputs, so after a
  warm reset -- which is how every flash ends -- the panel rail was already on and
  the panel kept its pre-reset state. This is the black screen Waveshare's FAQ
  answers with "disconnect and reconnect power", and it repeatedly caused good
  firmware changes to be blamed and reverted.
- The 7B source-built SDK now actually carries the vendor memory profile. It set
  the 64-byte cache line but silently kept ESP-IDF's default 32 KB data and 16 KB
  instruction caches against Waveshare's 64 KB and 32 KB. It also ran 120 MHz
  octal PSRAM without `CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR`,
  which ESP-IDF requires at that speed to avoid random failures once the die
  drifts ~20 C from power-on -- the normal case for an always-on device.
  `firmware/idf/main/sketch.cpp` now asserts every setting in the profile, and
  `scripts/build-idf.ps1` drops the cached sdkconfig when the defaults change.
- Stop rewriting the Now Playing elapsed-time label on every loop pass.
  `lv_label_set_text` reallocates and invalidates unconditionally, so writing the
  identical string kept the label redrawing and reflushing continuously; it now
  updates only when the displayed second changes.
- `scripts/flash.sh`, `monitor.sh` and `provision.sh` could not run on Windows:
  `resolve_port` hardcoded the POSIX `.venv/bin/python` path. It now also looks
  in `.venv/Scripts`.

### Changed

- Backends gained a `poll()` hook, called from the main loop, so diagnostics run
  outside LVGL's render and flush stack rather than inside the display hot path.
- The 7B logs sustained LVGL flush throughput into PSRAM every five seconds, so
  bus contention can be measured rather than inferred from the picture.
- Log the 7B RGB pixel clock and bounce-buffer size at boot for physical display diagnostics. A 20 MHz trial produced a black screen on the connected board; retain the original 30 MHz setting.
- Provisioning, API diagnostics, and artwork diagnostics now use an injected serial stream.
- The 7B uses UART1 / USB TO UART as its primary flash, monitor, and provisioning connection.
- The 7B detector now applies the documented GT911 reset/address-selection sequence before requiring its product ID, and RGB flushes use the ESP-IDF driver for PSRAM cache synchronization.
- The 7B retains Waveshare's ten-scanline DMA bounce buffers and keeps the 96 KB LVGL object heap in PSRAM. The twenty- and thirty-line trials were each judged by a single post-flash boot, which is exactly the warm-reset condition that produces a black screen on this board; neither was shown to cause one.
- macOS bootstrap now downloads the correct Arduino CLI archive.

### Validation

- Native and Python tests pass and the universal N16R8 firmware compiles.
- The source-built 7B firmware runs on hardware with a working display. Measured
  idle flush throughput rose from 14,033 KB/s to 54,065 KB/s, and time spent in
  flush fell from 83 ms to 22 ms per five-second window -- the latter being bus
  time taken away from the panel's refill ISR. Four changes landed together, so
  no single one is credited. Behaviour under Spotify load is not yet measured,
  and the original black screen is not yet explained.
- UART1 auto-selection, flash verification, post-flash provisioning, expander/GT911 detection, remembered `Wide7B` verification, RGB panel allocation, and backlight enable pass on SKU 31726.
- Physical validation remains recorded per board in `docs/hardware-checklist.md`.

## [1.0.0-rc.2] - 2026-09-06

Second release candidate with physical-device fixes and interaction polish.

### Added

- Immediate button feedback and short directional animations for previous/next swipes.
- TLS error codes and heap diagnostics in serial output for failed Spotify connections.
- A design for extending the firmware to additional board profiles.

### Changed

- Prefetch the first playlist page after authorization and reuse it when the library opens.
- Prioritize Spotify API and playback work over artwork network activity.
- Use lightweight audio/play symbols on playlist and song rows. Repeated row-artwork TLS handshakes fragmented internal memory on the ESP32-S3; full-size Now Playing artwork remains enabled.

### Fixed

- Release artwork connections before Spotify API handshakes so their TLS memory is available.
- Allow failed playlist prefetches to retry instead of leaving the library permanently pending.
- Keep a single screen gesture handler across view rebuilds.
- Send an explicit zero content length for bodyless Spotify write requests.
- Diagnose inaccessible serial ports before starting browser authorization.

### Validation

- Native C++ tests, Python provisioning tests, and the complete firmware build pass.
- Firmware upload and flash verification pass on the Waveshare ESP32-S3-Touch-LCD-2.
- The remaining physical interaction and soak checks are tracked in `docs/hardware-checklist.md`.

## [1.0.0-rc.1] - 2026-09-05

Initial public release candidate.

### Added

- Touchscreen playback controls for play/pause, previous, next, seek, volume, shuffle, and repeat.
- Spotify Connect device discovery and playback transfer.
- Playlist, song, and Liked Songs browsing with bounded pagination.
- Cover-first Now Playing interface with in-memory artwork handling.
- Browser-based Spotify PKCE authorization and USB provisioning without a client secret.
- Automatic token refresh, validated HTTPS, rate-limit handling, offline recovery, and factory-reset recovery.
- Native C++ tests, Python provisioning tests, reproducible firmware builds, and public CI.
- MIT license.

### Release candidate limitations

- Physical acceptance testing is pending because the test antenna arrived damaged.
- Hardware-specific issues may remain until every item in `docs/hardware-checklist.md` has passed.
- No firmware binary is attached; build from source with the pinned local toolchain.

### Requirements

- Waveshare ESP32-S3-Touch-LCD-2.
- Spotify Premium and a Spotify developer application.
- 2.4 GHz Wi-Fi and an existing Spotify Connect playback device.

The controller controls playback on another Spotify Connect device; it does not output audio.
