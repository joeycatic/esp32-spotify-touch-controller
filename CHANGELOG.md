# Changelog

All notable changes to this project are documented here.

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
