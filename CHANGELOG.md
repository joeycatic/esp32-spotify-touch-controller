# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added

- One universal ESP32-S3 firmware image for the 2-inch ST7789/CST816 board and the 7B 1024×600 RGB/GT911 board.
- Conservative I²C hardware detection, verified remembered profiles, recoverable headless provisioning, and profile-specific serial routing.
- Native 7B focused-split player, persistent bottom navigation, 64-pixel controls, wide library/device/QR views, and on-screen hold-to-reset.
- Shared USB serial resolver that recognizes Espressif native USB and the 7B CH343 UART adapter while ignoring macOS pseudo-ports.
- Profile-driven exact artwork sizes and wide-only bounded row thumbnails.

### Changed

- Provisioning, API diagnostics, and artwork diagnostics now use an injected serial stream.
- The 7B uses UART1 / USB TO UART as its primary flash, monitor, and provisioning connection.
- The 7B detector now applies the documented GT911 reset/address-selection sequence before requiring its product ID, and RGB flushes use the ESP-IDF driver for PSRAM cache synchronization.
- The 7B retains Waveshare's ten-scanline DMA bounce buffers for smooth RGB output and the 96 KB LVGL object heap required by wide-layout screen rebuilds.
- macOS bootstrap now downloads the correct Arduino CLI archive.

### Validation

- Native and Python tests pass and the universal N16R8 firmware compiles.
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
