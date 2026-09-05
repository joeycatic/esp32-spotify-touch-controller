# Changelog

All notable changes to this project are documented here.

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

### Release candidate limitations

- Physical acceptance testing is pending because the test antenna arrived damaged.
- Hardware-specific issues may remain until every item in `docs/hardware-checklist.md` has passed.
- No firmware binary is attached; build from source with the pinned local toolchain.

### Requirements

- Waveshare ESP32-S3-Touch-LCD-2.
- Spotify Premium and a Spotify developer application.
- 2.4 GHz Wi-Fi and an existing Spotify Connect playback device.

The controller controls playback on another Spotify Connect device; it does not output audio.
