<div align="center">
  <h1>ESP32 Spotify Touch Controller</h1>
  <p>A standalone touchscreen remote for Spotify Connect, built for the Waveshare ESP32-S3-Touch-LCD-7B.</p>
  <p>
    <a href="https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/joeycatic/esp32-spotify-touch-controller/actions/workflows/ci.yml/badge.svg"></a>
    <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
    <img alt="Board: Waveshare ESP32-S3-Touch-LCD-7B" src="https://img.shields.io/badge/board-Waveshare%207B-1f6feb">
    <img alt="Status: release candidate" src="https://img.shields.io/badge/status-release%20candidate-orange">
  </p>
</div>

The controller connects directly to Wi-Fi and Spotify after a one-time setup over USB. It controls playback on a separate Spotify Connect device; **the ESP32 does not play audio**. Once configured, no computer, Raspberry Pi, or cloud relay needs to stay online.

## At a glance

| | Details |
| --- | --- |
| **Board** | Waveshare ESP32-S3-Touch-LCD-7B, SKU 31726, 1024 × 600 display |
| **Playback** | Controls Spotify on a phone, computer, speaker, TV, or other Spotify Connect device |
| **Network** | 2.4 GHz Wi-Fi and internet access |
| **Account** | Spotify Premium and a Spotify developer application |
| **Setup** | Browser-based Spotify authorization and local USB serial provisioning |

## What it can do

- Control playback, seek, volume, shuffle, and repeat.
- Choose a Spotify Connect output and transfer playback to it.
- Browse playlists, songs, and Liked Songs; view the up-next queue and add songs to it.
- Save or remove the playing track or episode from your Spotify library.
- Show album art, load playlist thumbnails as needed, and open the current Spotify item with a QR code.
- Recover setup from the touchscreen or USB serial if Wi-Fi or Spotify authorization needs to be changed.

## Requirements

- A Waveshare **ESP32-S3-Touch-LCD-7B (SKU 31726)**. The former 240 × 320 board is no longer supported.
- Linux, macOS, or Windows with WSL, plus Python 3, `curl`, `tar`, `git`, `g++`, and a USB-C data cable.
- A 2.4 GHz Wi-Fi network with internet access.
- Spotify Premium, a Spotify developer application with Web API access, and another Spotify Connect device for audio playback.

For Spotify Development Mode, the app owner needs Premium and each account using the app must be on its allowlist (up to five users). See [Spotify's Development Mode requirements](https://developer.spotify.com/documentation/web-api/concepts/quota-modes).

## Set up

The recommended firmware for the 7B is the source-built ESP-IDF target. It rebuilds the SDK with the memory settings needed by the RGB display. The faster Arduino CLI target (`make build` / `make flash`) is retained for development, but is not the target to flash to this board.

1. Install the pinned project tools and run the automated checks:

   ```bash
   make bootstrap
   make test
   ```

2. Install ESP-IDF **v5.5.5** and its ESP32-S3 tools, then build the firmware. See the [ESP-IDF build guide](firmware/idf/README.md) for the platform-specific setup.

   ```bash
   make build-idf
   ```

3. Follow the [setup guide](docs/setup.md) to flash the board. Connect the cable to **UART1 / USB TO UART** and set the nearby DIP switch to route the adapter to the ESP32. The checked flash procedure requires a verified recovery backup; the guide explains the safeguards and platform-specific commands.

4. Create a Spotify developer application and add this redirect URI exactly:

   ```text
   http://127.0.0.1:8765/callback
   ```

5. With the board connected, run provisioning:

   ```bash
   make provision
   ```

   Enter the Client ID and Wi-Fi details when prompted, then approve access in your browser. The utility uses PKCE and does not require a client secret. Start Spotify on your audio device; the controller will find its playback session.

The [full setup guide](docs/setup.md) covers Spotify developer settings, serial-port selection, first playback, reset and recovery, and troubleshooting.

## Touch controls

| Action | How |
| --- | --- |
| Open the library | Tap **Library** in the bottom navigation or swipe up |
| Change track | Swipe left or right on Now Playing, or use the playback controls |
| Browse a playlist | Tap it; some playlists are play-only because of Spotify development-mode access limits |
| Choose an audio output | Tap **Devices** or the device name in the top bar |
| Adjust volume | Tap **Volume**, then use the slider |
| Seek | Drag the progress bar and release |
| Save or remove the playing item | Tap the checkmark beside its title; available for tracks and episodes, not local files |
| Open the current Spotify item | Tap **QR** or the Spotify attribution |
| Return to the previous view | Use the back arrow or swipe down |

The gear menu opens diagnostics, the four-corner touch test, and setup reset. Hold **Reset setup** for three seconds to erase the saved configuration.

## Project status

This is a release candidate targeting the 7B only. On one physical 7B, the display, touch, playback controls, library, artwork loading, provisioning, and Wi-Fi recovery have been checked. A one-hour Spotify playback soak completed on **November 7, 2026** with a stable display and no recorded reset or panic. Expired-token refresh, revoked authorization, rate-limit recovery, and heap trends remain unchecked; see the [hardware checklist](docs/hardware-checklist.md) for results and remaining items.

## Known limits

- The controller sends playback commands to Spotify Connect; it has no audio output of its own.
- Spotify Premium is required for playback control. The ESP32-S3 supports 2.4 GHz Wi-Fi, not a 5 GHz-only network.
- Spotify development mode may hide tracks in followed playlists that you neither own nor collaborate on. These playlists remain available as play-only entries.
- Liked Songs playback queues the selected item and up to 49 currently loaded following songs; it does not create an unlimited queue.
- Global search, playlist editing, lyrics, podcast-library browsing, camera, sensors, and battery operation are outside this release.

## Security and credentials

The provisioning utility requests access with OAuth PKCE, uses verified HTTPS, and does not use a Spotify client secret. Wi-Fi details, the Spotify Client ID, and the refresh token are stored in versioned NVS slots on the ESP32; access tokens are held in RAM. **Flash encryption and secure boot are not enabled**, so someone with physical access and suitable equipment may be able to extract stored credentials. Never commit or share provisioning data, tokens, or Wi-Fi passwords. See [architecture and security](docs/architecture.md) for details.

## Development

| Command | Purpose |
| --- | --- |
| `make bootstrap` | Install pinned Arduino libraries and the local provisioning environment |
| `make test` | Run native C++ and Python checks |
| `make build-idf` | Build the recommended 7B firmware with source-built ESP-IDF |
| `make flash-idf` | Flash the source-built firmware using the checked procedure |
| `make provision` | Configure Spotify and Wi-Fi over USB serial |
| `make monitor` | Open the sanitized 115200-baud serial monitor |

Read [contributing](CONTRIBUTING.md) before opening a pull request. Report security issues privately using [SECURITY.md](SECURITY.md).

## License

Released under the [MIT License](LICENSE).
