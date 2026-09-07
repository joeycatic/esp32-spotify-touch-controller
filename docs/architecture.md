# Architecture and Security

## Universal Board Startup

One N16R8 firmware image supports two immutable runtime profiles. Before LVGL or any display bus starts, `BoardDetector` verifies the profile remembered in the separate `spotctl_hw` NVS namespace. If needed, it probes the 7B I²C bus on GPIO8/9 for both the Waveshare `0x24` expander protocol and GT911 product ID `911`, then probes the compact CST816 identity on GPIO48/47. No match or an ambiguous match selects no backend.

`Board` is the application-facing facade. `Compact2Backend` preserves Arduino_GFX/ST7789, CST816, GPIO1 backlight, and 240×320 behavior. `Wide7BBackend` owns the 1024×600 ESP-IDF RGB panel, GT911, and Waveshare I/O expander. The expander driver is intentionally named by board rather than chip: Waveshare documentation calls it CH422G while the current schematic/example exposes a custom `0x24` register protocol.

Both serial transports start before detection. Compact uses native USB; 7B uses UART0 through its CH343-backed UART1 connector. Boot diagnostics go to both. Unknown hardware starts a headless UART0 provisioning/recovery loop instead of driving unverified display pins.

## Runtime Flow

The Arduino loop owns LVGL and never performs Spotify HTTPS requests. Touch callbacks create small `UiCommand` values, which are transferred through a mutex-protected queue to a FreeRTOS task pinned to core 0. That task owns token refresh, API requests, pagination, rate-limit state, and JPEG downloads. It returns copied `NetworkEvent` values to the UI loop. The first playlist page is requested after authorization and retained by the UI, so opening the library normally requires no network round trip.

Now Playing is polled every two seconds while playing, every five seconds while paused, and every fifteen seconds without an active item. The displayed progress advances from the last observed Spotify timestamp between polls. Successful control commands cause a reconciliation poll after 400 ms.

## Memory

- LVGL uses two 40-row RGB565 draw buffers. They occupy about 19 KB on compact and 160 KB on 7B. Compact may fall back to internal memory; the 7B requires PSRAM.
- The 7B RGB peripheral owns a 1,228,800-byte PSRAM framebuffer and Waveshare's two 10-row internal DMA bounce buffers for stable 30 MHz scanout. Firmware submits completed LVGL areas through the RGB driver so PSRAM cache synchronization occurs before scanout. LVGL retains its 96 KB object heap because the native wide layout exceeds a smaller allocation during screen rebuilds.
- Current artwork is downloaded into PSRAM and decoded into an immutable RGB565 frame. A reference-counted handle crosses the network/UI queue so LVGL can never draw from a frame that the other core has freed or reused.
- Playback events are coalesced, limiting artwork ownership to the frame displayed by the UI and at most one pending replacement.
- Player artwork is resampled once into an exact 184×184 compact or 400×400 wide RGB565 frame. Spotify's largest image URL is retained for the wide player.
- Spotify API work closes any retained artwork connection before opening its TLS session. Row artwork remains disabled on compact. The 7B queues only visible 64×64 covers, fetches one per idle pass, uses a bounded LRU, and releases TLS after each batch.
- Artwork is never written to flash or a filesystem.
- Playlist and track screens retain at most three 20-item pages.
- Liked Songs playback sends no more than 50 locally loaded URIs.

## Credentials

The desktop utility uses Spotify Authorization Code with PKCE. The authorization code, temporary access token, Wi-Fi password, and refresh token are never logged. The ESP32 stores Wi-Fi credentials, Client ID, and refresh token in versioned A/B NVS slots and writes the schema marker last before switching the active slot.

Access tokens exist only in RAM. If Spotify returns a rotated refresh token, it is written to the inactive NVS slot and verified before activation.

NVS is not encrypted in this hobby-device build. Someone with physical access and suitable hardware may extract stored credentials. Flash encryption and secure boot are deliberately excluded because provisioning them can irreversibly burn eFuses.

## Transport Security

All Spotify API, account, and artwork requests use `NetworkClientSecure` with the certificate bundle included by Arduino-ESP32. Certificate verification is never disabled. NTP synchronization must succeed before OAuth or API traffic begins.

An API `401` refreshes and retries once. A revoked or otherwise invalid refresh token switches the device directly back to serial provisioning mode. A `429` suspends requests for Spotify’s `Retry-After` interval. Transient failures use jittered 2, 4, 8, 16, 32, and 60-second delays. Offline touch commands are rejected visibly instead of being queued for surprising later execution.

Provisioning uses the active board's serial `Stream`; it is not tied to global `Serial`. Host port discovery accepts only supported Espressif native USB or WCH CH343 metadata, ignores non-USB macOS pseudo-ports, prefers the 7B UART adapter, and requires an explicit override for ambiguity.

## Spotify Boundaries

The requested scopes are limited to playback state/control, private and collaborative playlist reading, and saved-library reading. No email, profile-writing, playlist-writing, or streaming scope is requested.

Album artwork is center-cropped only if Spotify supplies a non-square image, then resampled to the profile's exact square target without overlays. Tapping Spotify attribution or the wide QR navigation item displays the current Spotify URL.

## UI and Recovery

The UI owns shared playback state and semantic callbacks. Compact retains the original portrait coordinates. Wide screens use a 64-pixel top bar, focused 400-pixel artwork/player split, minimum 64-pixel controls, and a persistent 72-pixel bottom navigation bar.

Compact factory reset remains a ten-second BOOT hold. On 7B, BOOT is sampled before GPIO0 becomes RGB G3; a boot-time hold enters provisioning, while credential erasure requires holding the on-screen reset control for three seconds. Credential erasure clears `spotctl` but leaves the independently verified hardware hint in `spotctl_hw`.
