# ESP32 Spotify Touch Controller

[![CI](https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml/badge.svg)](https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml)

> [!WARNING]
> `v1.0.0-rc.1` is a release candidate. Native tests and firmware compilation pass, but physical acceptance testing on the Waveshare ESP32-S3-Touch-LCD-2 is pending because the test antenna arrived damaged. Expect hardware-specific issues until the [hardware checklist](docs/hardware-checklist.md) is complete.

A standalone Spotify display and touchscreen remote for the Waveshare ESP32-S3-Touch-LCD-2. After one-time USB setup, the controller connects directly to Wi-Fi and Spotify. No computer, cloud relay, or Raspberry Pi needs to remain running.

The ESP32 controls Spotify playback on another Spotify Connect device. It does not play audio itself.

## Project Status

The software is feature-complete for the first release candidate. Physical-device validation is still pending; see the [hardware acceptance checklist](docs/hardware-checklist.md) for the exact tests that remain.

## Features

- Cover-first 240×320 Now Playing screen with memory-only album artwork
- Play/pause, previous, next, seek, volume, shuffle, and repeat
- Spotify Connect output-device picker and playback transfer
- Playlist and song browsing with 20-item pagination
- Liked Songs browsing and selection
- Play-only handling for followed playlists whose items Spotify hides
- Swipe-up library, mini-player, and Open-in-Spotify QR code
- One-time browser PKCE authorization over USB serial; no client secret
- Automatic access-token refresh and rotated refresh-token storage
- Validated HTTPS, rate-limit handling, offline recovery, and bounded backoff
- Four-corner touch diagnostic and physical factory-reset recovery

## Quick Start

Requirements: Linux, macOS, or WSL with Python 3, `curl`, `tar`, `git`, `g++`, and a USB-C data cable.

```bash
make bootstrap
make test
make build
make flash
```

Create a Spotify developer application, add this exact redirect URI, and then run provisioning:

```text
http://127.0.0.1:8765/callback
```

```bash
make provision
```

The setup utility asks for the Spotify Client ID, Wi-Fi network, and Wi-Fi password locally. It opens Spotify authorization in the browser, sends only the resulting refresh token and configuration over USB, and waits for the ESP32 to confirm storage.

Detailed instructions are in [docs/setup.md](docs/setup.md). Architecture and security decisions are documented in [docs/architecture.md](docs/architecture.md), and the physical-device test procedure is in [docs/hardware-checklist.md](docs/hardware-checklist.md).

## Common Commands

```bash
make bootstrap                         # Install pinned tools under this repository
make test                              # Native C++ and Python tests
make build                             # Compile the ESP32 firmware
make flash                             # Build and upload to the detected board
PORT=/dev/ttyACM0 make flash           # Upload using an explicit serial port
make monitor                           # Sanitized 115200-baud serial monitor
make provision                         # Interactive one-time setup
./scripts/provision.sh --port /dev/ttyACM0
```

## Important Limits

- Spotify Premium is required for playback control.
- The ESP32-S3 supports 2.4 GHz Wi-Fi, not a 5 GHz-only network.
- Internet access is required during normal use.
- Global search, playlist editing, lyrics, podcast-library browsing, camera, sensors, battery operation, and audio output are outside version 1.
- Spotify development mode can hide song lists for followed playlists that the user does not own or collaborate on. Those playlists remain available as play-only entries.
- Liked Songs selection sends the selected song and up to 49 currently loaded following songs. It does not rebuild an unlimited library queue.

## Credential Safety

Do not place Wi-Fi passwords, access tokens, refresh tokens, or client secrets in project files, shell arguments, issue reports, or chat. This project does not use a Spotify client secret. Configuration is stored in versioned A/B NVS slots; physical extraction remains possible because flash encryption is intentionally not enabled and no security eFuses are burned.

## Contributing and Security

Contributions are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Report security issues privately as described in [SECURITY.md](SECURITY.md), and never include credentials or tokens in an issue.
