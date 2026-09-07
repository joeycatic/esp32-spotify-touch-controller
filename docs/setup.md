# Setup Guide

## 1. Install the Local Toolchain

From the repository root:

```bash
make bootstrap
```

This installs Arduino CLI 1.5.1, Arduino-ESP32 3.3.11, LVGL 8.4.0, Arduino_GFX 1.6.7, ArduinoJson 7.4.3, TJpg_Decoder 1.1.0, and pyserial 3.5 under ignored project directories.

Arduino_GFX 1.6.7 is intentional. The 1.5.0 version in the original design predates an ESP32 SPI API change and does not compile with Arduino-ESP32 3.3.11.

Verify the software before connecting an account:

```bash
make test
make build
```

## 2. Flash the Board

Connect the board with a USB-C data cable:

- **ESP32-S3-Touch-LCD-7B:** use the connector labeled **UART1 / USB TO UART**. Set the nearby DIP switch to route the CH343 adapter to the ESP32, not the external UART header.
- **ESP32-S3-Touch-LCD-2:** use its regular USB connector.

The 7B connector labeled USB is multiplexed with CAN through the board I/O expander and normally does not enumerate before firmware configures it. UART1 has the automatic download circuit and is the reliable flashing connector.

```bash
make flash
```

If more than one serial device is connected, specify it:

```bash
PORT=/dev/cu.usbserial-DEVICE make flash
```

The host resolver ignores Bluetooth and debug pseudo-ports, recognizes Espressif native USB and the 7B CH343 adapter, and prefers UART1 when both 7B ports exist. It refuses to guess if multiple matching devices remain.

At boot, firmware verifies a remembered hardware profile, probes the 7B on GPIO8/9, and then probes the compact CST816 on GPIO48/47. It initializes a display only after a positive identity match. An unconfigured controller displays “Connect USB to set up”; an unidentified or failed display remains available for headless provisioning through UART1.

## 3. Create the Spotify Application

1. Sign in to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Create an application for personal use and enable Web API access.
3. Add this redirect URI exactly:

   ```text
   http://127.0.0.1:8765/callback
   ```

4. Save the settings.
5. Add the Spotify account as an allowed development user when required by the dashboard.
6. Copy the application Client ID. A client secret is neither needed nor accepted by this project.

## 4. Provision over Serial

Leave the board connected and run:

```bash
make provision
```

The utility will:

1. Ask for the Client ID, 2.4 GHz Wi-Fi SSID, and Wi-Fi password.
2. Open Spotify authorization in the default browser.
3. Receive the local callback and verify its random OAuth state.
4. Exchange the authorization code using PKCE.
5. Verify the account with Spotify.
6. Detect the supported ESP32/CH343 serial port.
7. Send the validated provisioning record and wait for acknowledgement.

If port detection is ambiguous:

```bash
PORT=/dev/cu.usbserial-DEVICE make provision
# or
./scripts/provision.sh --port /dev/cu.usbserial-DEVICE
```

Credentials are entered locally. They are never generated as files and the ESP32 never echoes them.

## 5. First Playback

Open Spotify on a phone, computer, television, or Spotify Connect speaker and start a song. The ESP32 should show playback within the active two-second polling interval. Tap the device name at the top of Now Playing to choose a different output.

Controls:

- Swipe up: open playlists and Liked Songs.
- Swipe left/right on Now Playing: next/previous track.
- Tap a playlist: browse songs when Spotify permits it.
- Tap a play-only playlist: start the whole playlist.
- Tap a song: start at that song.
- Tap the device name: output-device picker.
- Tap the volume icon: volume slider.
- Drag the progress bar and release: seek.
- Tap SPOTIFY: show a QR link for the current item.
- Swipe down or use the back arrow: return.

On the 7B, the bottom navigation provides Now Playing, Library, Devices, Volume, and QR directly. The gear in the top bar opens diagnostics, touch testing, and setup reset.

## Reprovisioning and Recovery

- Change Wi-Fi or renew authorization: reset while holding BOOT, then run `make provision`. On the 7B, BOOT is sampled only at startup because GPIO0 becomes an RGB display signal.
- 2-inch factory reset: reset while holding BOOT and continue holding for ten seconds. The screen shows the countdown before deleting saved credentials.
- 7B factory reset: open the gear screen and hold **Reset setup** for three seconds. Releasing early cancels it.
- Revoke access remotely: remove the application under the Spotify account’s connected-app settings. The controller will request USB authorization again after refresh fails.

## Troubleshooting

### No serial device

For a 7B, first confirm the cable is in **UART1 / USB TO UART**, the DIP switch targets the ESP32, and the cable supports data. For a 2-inch board, use its normal USB port. Inspect USB-backed ports with:

```bash
PYTHONPATH=tools/provision .venv/bin/python -m spotify_provision.ports
```

To recover the 7B through its regular USB connector, hold BOOT while reconnecting USB and keep it held until a download begins. Prefer UART1 for normal use.

On Fedora, add the current account to the serial-access group if the device is owned by `root:dialout`, then log out and back in:

```bash
sudo usermod -aG dialout "$(id -un)"
```

Group membership only reaches a session at login, so a shell opened before that
command still fails with `Permission denied: /dev/ttyACM0` even though `getent
group dialout` lists the account. Log out and back in, or run a single command
with the group applied:

```bash
sg dialout -c 'make provision'
```

Avoid running provisioning as root because it opens a browser and handles account credentials.

### Spotify callback rejected

Verify the dashboard redirect URI is exactly `http://127.0.0.1:8765/callback`; `localhost`, another port, and a trailing slash are different redirect URIs.

### No active device

Start Spotify on another device first, then open the controller’s device picker. The ESP32 is not itself an audio destination.

### Playlist says play only

This is an expected Spotify development-mode restriction for some followed non-owned playlists. Owned and collaborative playlists expose their song lists.

### Playlist or Liked Songs does not load

Use `v1.0.0-rc.2` or newer. Earlier builds could leave too little contiguous internal memory for a Spotify TLS handshake after loading row artwork. The current build prefetches and caches the playlist page, gives API traffic priority, keeps compact rows as symbols, and bounds 7B thumbnail work. Run `make monitor` to see a TLS error code and heap diagnostics if a request still fails.

### Artwork is missing

Playback controls remain usable when an image URL is absent, decoding fails, or PSRAM is unavailable. A failed image download is retried on later playback polls; changing tracks also triggers a fresh load.

Artwork is shown on Now Playing. The compact profile intentionally uses symbols on playlist/song rows to preserve contiguous memory for Spotify API connections. The 7B loads only visible 64-pixel row covers through a bounded cache, one idle download per pass.
