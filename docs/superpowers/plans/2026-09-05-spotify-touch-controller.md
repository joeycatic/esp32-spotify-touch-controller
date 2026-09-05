# ESP32 Spotify Touch Controller Implementation Plan

Build a standalone, USB-powered Spotify display and remote for the Waveshare ESP32-S3-Touch-LCD-2. After one-time setup it connects directly to 2.4 GHz Wi-Fi and Spotify; the computer is no longer required. Audio remains on an existing Spotify Connect device.

## Fixed Technical Decisions

- Arduino-ESP32 3.3.11, LVGL 8.4.0, ArduinoJson 7.4.3, TJpg_Decoder 1.1.0, and a project-local Arduino CLI toolchain.
- ST7789T3 240×320 portrait display on SCLK 39, MOSI 38, MISO 40, DC 42, CS 45, and backlight 1.
- CST816D touch at I²C address `0x15`, SDA 48, SCL 47.
- Direct certificate-validated HTTPS and Authorization Code with PKCE. No client secret and no always-running helper service.
- Wi-Fi, Client ID, and refresh token stored atomically in versioned A/B NVS slots. Access tokens remain in RAM.

## Delivery Phases

1. Bootstrap pinned local tools and repeatable build, test, flash, monitor, and provision commands.
2. Add board support, LVGL buffering, portrait touch, hardware diagnostics, BOOT recovery, and PSRAM checks.
3. Add platform-independent playback/library models, parsing, state reduction, polling, interpolation, backoff, and pagination policies.
4. Add the Python PKCE callback utility and newline-delimited USB provisioning protocol with strict state, field, and secret-redaction checks.
5. Add refresh-token rotation, Spotify playback commands, devices, playlists, playlist items, Liked Songs, and resilient HTTP error handling.
6. Add the cover-first Now Playing UI, library/song selection, mini-player, device and volume screens, touch diagnostics, persistent offline state, and Spotify QR view.
7. Add immutable shared artwork frames, cross-core queues, memory bounds, retry behavior, and security hardening.
8. Run native/Python tests and a clean ESP32 compile, then hand off the physical-device acceptance checklist.

## Acceptance Boundaries

Host verification covers deterministic core logic, PKCE/callback validation, provisioning framing and redaction, Spotify response/error parsing, request encoding, pagination, and the pinned firmware build. Final display, touch, Wi-Fi, account, device-transfer, persistence, and one-hour soak checks require the physical board and are tracked in `docs/hardware-checklist.md`.

Version 1 deliberately excludes audio output, global search, playlist editing, lyrics, podcast-library browsing, camera/sensor use, SD card, and battery management. It does not burn security eFuses or enable flash encryption.

Implementation note: Arduino_GFX 1.5.0 from the original plan does not compile with Arduino-ESP32 3.3.11 because Espressif changed `spiFrequencyToClockDiv` in core 3.3.6. The project therefore pins Arduino_GFX 1.6.7, which contains the upstream compatibility fix.
