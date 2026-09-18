# Architecture and Security

## Universal Board Startup

One N16R8 firmware image targets one board. There is no runtime detection: `Wide7BBackend` brings up the 7B's I²C bus on GPIO8/9, the Waveshare `0x24` expander protocol and the GT911 directly. An earlier revision carried a second 240×320 profile and a `BoardDetector` that chose between them; both were removed after `552ef21`, together with the `spotctl_hw` NVS namespace that remembered the verified profile.

`Board` is the application-facing facade and owns a concrete `Wide7BBackend`, which drives the 1024×600 ESP-IDF RGB panel, GT911, and Waveshare I/O expander. The `BoardBackend` virtual interface was removed with the second board; with one implementation it only hid which panel the caller was talking to. The expander driver is intentionally named by board rather than chip: Waveshare documentation calls it CH422G while the current schematic/example exposes a custom `0x24` register protocol.

The console is UART0 through the CH343-backed UART1 connector; the board's own USB port is shared with CAN and may not enumerate before firmware starts. Boot diagnostics go to both `Serial` and `Serial0`. A display that fails to initialize starts a headless UART0 provisioning/recovery loop.

## Runtime Flow

The Arduino loop owns LVGL and never performs Spotify HTTPS requests. Touch callbacks create small `UiCommand` values, which are transferred through a mutex-protected queue to a FreeRTOS task pinned to core 0. That task owns token refresh, API requests, pagination, rate-limit state, and JPEG downloads. It returns copied `NetworkEvent` values to the UI loop. The first playlist page is requested after authorization and retained by the UI, so opening the library normally requires no network round trip.

Now Playing is polled every two seconds while playing, every five seconds while paused, and every fifteen seconds without an active item. The displayed progress advances from the last observed Spotify timestamp between polls. Successful control commands cause a reconciliation poll after 400 ms.

## Memory

- LVGL draw buffers are RGB565 and allocated from internal RAM first, PSRAM only as a degraded fallback that the boot log names. One 16-row buffer, 32 KB: a second buffer buys nothing against a synchronous flush, and it must stay small enough to fit internal RAM beside the panel's bounce buffers. Rendering into PSRAM instead makes each flush a PSRAM-to-PSRAM copy on the same bus the panel's DMA is already saturating, which measured 14 MB/s against 54 MB/s from internal RAM.
- The 7B RGB peripheral owns a 1,228,800-byte PSRAM framebuffer and Waveshare's two 10-row internal DMA bounce buffers for stable 30 MHz scanout. Enlarging those buffers is not a fix: the refill deadline is not the binding constraint, PSRAM read bandwidth is, and the larger buffers cost internal RAM the draw buffer and caches need. Firmware submits completed LVGL areas through the RGB driver, which in bounce-buffer mode copies line by line and deliberately skips the cache sync, because the refill ISR reads the framebuffer through the same cache. Sustaining that refill depends on SDK-level PSRAM clock and cache geometry that Arduino CLI's precompiled libraries fix; `firmware/idf` exists to rebuild them. LVGL retains a bounded 96 KB object heap in PSRAM because the native wide layout exceeds a smaller allocation during screen rebuilds; keeping that pool out of internal DRAM preserves a contiguous block for TLS.
- Current artwork is downloaded into PSRAM and decoded into an immutable RGB565 frame. A reference-counted handle crosses the network/UI queue so LVGL can never draw from a frame that the other core has freed or reused.
- Playback events are coalesced, limiting artwork ownership to the frame displayed by the UI and at most one pending replacement.
- Player artwork is resampled once into an exact 400×400 RGB565 frame. Spotify's largest image URL is retained.
- Spotify API work closes any retained artwork connection before opening its TLS session. Row artwork queues only visible 64×64 covers, fetches one per idle pass, uses a bounded LRU, and releases TLS after each batch.
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

The UI owns shared playback state and semantic callbacks. The screen uses a 64-pixel top bar, focused 400-pixel artwork/player split, minimum 64-pixel controls, and a persistent 72-pixel bottom navigation bar. On the 7B, Library uses two-column playlist cards with a separate Liked Songs action; playlist tracks use full-width rows with separate title, artist, duration, and current-track states. Shared static LVGL styles keep those repeated rows from allocating a new style for every item.

BOOT is sampled before GPIO0 becomes RGB G3; a boot-time hold enters provisioning, while credential erasure requires holding the on-screen reset control for three seconds. The ten-second BOOT-hold reset belonged to the removed 240×320 board and is gone. Credential erasure clears the `spotctl` namespace.
