# Hardware Acceptance Checklist

Automated checks use `make test && make build`. Physical checks require Spotify Premium, 2.4 GHz Wi-Fi, and a Spotify Connect playback device. Record observations only on the named board; a previous compact upload is not post-refactor acceptance.

## Universal automated checks

- [x] Detector policy covers 7B, compact, partial, no-match, and ambiguous evidence.
- [x] Profile display/media/reset/serial constants and wide layout bounds are tested.
- [x] Host resolver covers Espressif USB, CH343 preference, pseudo-ports, overrides, and ambiguity.
- [x] Native C++ and Python provisioning suites pass.
- [x] One N16R8 firmware image compiles for both profiles.

## ESP32-S3-Touch-LCD-7B (SKU 31726)

### Bring-up and provisioning

- [x] UART1 / USB TO UART enumerates through the onboard CH343 and `make flash` selects it.
- [x] Upload and flash verification complete without manually specifying `PORT`.
- [x] Cold boot and a remembered-profile reboot both report `Wide7B`.
- [ ] Display is 1024×600 landscape with correct colors, no drift/flicker/tearing, and stable backlight.
- [ ] GT911 reports accurately at all four corners and gestures track the expected direction.
- [ ] Runtime native USB appears after the expander selects USB; UART1 remains usable for logs/provisioning.
- [x] `make provision` succeeds after flashing, acknowledges storage, and survives a reset.
- [x] Serial output contains no Wi-Fi password, authorization code, access token, or refresh token.

### UI and Spotify

- [ ] Top status bar, 400×400 artwork, player metadata, progress, and 64-pixel controls fit without clipping.
- [ ] Persistent Now Playing, Library, Devices, Volume, and QR navigation works.
- [ ] Play/pause, previous, next, seek, volume, shuffle, repeat, device transfer, and QR link work.
- [ ] Owned/collaborative playlists, play-only playlists, and Liked Songs behave correctly.
- [ ] Visible 64×64 row thumbnails load incrementally without blocking controls.
- [ ] Gear diagnostics and four-corner touch test work.
- [ ] Holding on-screen Reset setup for three seconds erases credentials; releasing early cancels.
- [ ] Holding BOOT during reset enters provisioning without continuously reading GPIO0 after RGB starts.

### Recovery and soak

- [ ] Wi-Fi loss/recovery, expired-token refresh, revoked authorization, and `429 Retry-After` recover correctly.
- [ ] Unknown/touch/display failure paths remain available over sanitized serial recovery.
- [ ] One-hour playback/library/artwork soak has no reboot, frozen touch, corrupt frame, or unbounded memory loss.

## ESP32-S3-Touch-LCD-2 regression

- [x] A pre-refactor firmware upload and flash verification passed on this model.
- [ ] Universal firmware detects `Compact2` on cold and remembered-profile boots.
- [ ] Portrait orientation, colors, backlight, and CST816 four-corner touch match the baseline.
- [ ] Native USB flash, monitor, and provisioning work automatically.
- [ ] Existing Now Playing, swipes, library, mini-player, devices, volume, and QR behavior is unchanged.
- [ ] Compact rows retain symbols and full player artwork remains stable.
- [ ] BOOT enters provisioning and a continuous ten-second hold shows the countdown and erases credentials.
- [ ] One-hour playback/navigation soak passes without reboot, frozen touch, or memory loss.
