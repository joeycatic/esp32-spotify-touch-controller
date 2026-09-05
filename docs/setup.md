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

Connect the Waveshare ESP32-S3-Touch-LCD-2 using a USB-C data cable:

```bash
make flash
```

If more than one serial device is connected, specify it:

```bash
PORT=/dev/ttyACM0 make flash
```

An unconfigured controller displays “Connect USB to set up.” Tap the hardware-status text at the bottom to open the four-corner touch test.

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

## 4. Provision over USB

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
6. Detect the ESP32 serial port.
7. Send the validated provisioning record and wait for acknowledgement.

If port detection is ambiguous:

```bash
./scripts/provision.sh --port /dev/ttyACM0
```

Credentials are entered locally. They are never generated as files and the ESP32 never echoes them.

## 5. First Playback

Open Spotify on a phone, computer, television, or Spotify Connect speaker and start a song. The ESP32 should show playback within the active two-second polling interval. Tap the device name at the top of Now Playing to choose a different output.

Controls:

- Swipe up: open playlists and Liked Songs.
- Tap a playlist: browse songs when Spotify permits it.
- Tap a play-only playlist: start the whole playlist.
- Tap a song: start at that song.
- Tap the device name: output-device picker.
- Tap the volume icon: volume slider.
- Drag the progress bar and release: seek.
- Tap SPOTIFY: show a QR link for the current item.
- Swipe down or use the back arrow: return.

## Reprovisioning and Recovery

- Change Wi-Fi or renew authorization: reset the board while holding BOOT, release BOOT, then run `make provision`.
- Factory reset: reset while holding BOOT and continue holding for ten seconds. The screen shows the countdown before deleting stored Wi-Fi and Spotify credentials.
- Revoke access remotely: remove the application under the Spotify account’s connected-app settings. The controller will request USB authorization again after refresh fails.

## Troubleshooting

### No serial device

Confirm the cable supports data and inspect available ports:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

On Fedora, add the current account to the serial-access group if the device is owned by `root:dialout`, then log out and back in:

```bash
sudo usermod -aG dialout "$(id -un)"
```

Avoid running provisioning as root because it opens a browser and handles account credentials.

### Spotify callback rejected

Verify the dashboard redirect URI is exactly `http://127.0.0.1:8765/callback`; `localhost`, another port, and a trailing slash are different redirect URIs.

### No active device

Start Spotify on another device first, then open the controller’s device picker. The ESP32 is not itself an audio destination.

### Playlist says play only

This is an expected Spotify development-mode restriction for some followed non-owned playlists. Owned and collaborative playlists expose their song lists.

### Artwork is missing

Playback controls remain usable when an image URL is absent, decoding fails, or PSRAM is unavailable. A failed image download is retried on later playback polls; changing tracks also triggers a fresh load.
