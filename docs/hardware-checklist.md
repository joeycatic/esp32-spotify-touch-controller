# Hardware Acceptance Checklist

Code-level checks can run without a board. The checks below require the physical Waveshare ESP32-S3-Touch-LCD-2, a Spotify Premium account, Wi-Fi, and at least one Spotify playback device.

## Bring-up

- [ ] `make flash` detects and uploads to the board.
- [ ] Startup reports a working display and CST816D touch controller.
- [ ] Startup reports 16 MB flash and 8 MB PSRAM.
- [ ] The display is portrait, colors are correct, and the backlight is stable.
- [ ] Tapping the hardware status opens the touch test.
- [ ] All four corner targets respond in the expected order.

## Provisioning

- [ ] `make provision` opens Spotify authorization.
- [ ] Wrong OAuth state is rejected.
- [ ] The ESP32 returns a successful provisioning acknowledgement.
- [ ] The board restarts and reconnects without the computer providing a service.
- [ ] A second power cycle proves the configuration persisted.
- [ ] Serial output contains no Wi-Fi password or Spotify token.

## Playback

- [ ] Now Playing appears within ten seconds after Wi-Fi is available.
- [ ] Artwork is square, correctly colored, and not cropped.
- [ ] Title, artist or show, progress, duration, device, shuffle, and repeat are accurate.
- [ ] Play/pause, previous, next, seek, volume, shuffle, and repeat work.
- [ ] Touch feedback is visible immediately and Spotify reconciles within three seconds.
- [ ] The QR code opens the correct current Spotify item.

## Library and Devices

- [ ] Owned playlist songs load and a selected song starts at its playlist position.
- [ ] Collaborative playlist songs load when one is available.
- [ ] A followed non-owned playlist displays its play-only explanation and starts.
- [ ] Liked Songs loads, paginates, and a selected song starts.
- [ ] The mini-player remains usable while browsing.
- [ ] Available playback devices load.
- [ ] Playback transfers between two devices.
- [ ] A restricted device is not offered as a valid transfer target.

## Recovery and Soak

- [ ] Turning Wi-Fi off retains the last display, marks the controller offline, and rejects commands.
- [ ] Restoring Wi-Fi reconnects without rebooting.
- [ ] An expired access token refreshes automatically.
- [ ] Revoked authorization displays the USB reauthorization screen.
- [ ] A simulated `429` respects `Retry-After`.
- [ ] Holding BOOT during reset enters provisioning without immediately erasing configuration.
- [ ] Holding BOOT for ten seconds displays a countdown and erases configuration.
- [ ] One hour of continuous playback causes no reboot, growing memory use, corrupt artwork, or frozen touch input.

