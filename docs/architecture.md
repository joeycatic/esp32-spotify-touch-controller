# Architecture and Security

## Runtime Flow

The Arduino loop owns LVGL and never performs Spotify HTTPS requests. Touch callbacks create small `UiCommand` values, which are transferred through a mutex-protected queue to a FreeRTOS task pinned to core 0. That task owns token refresh, API requests, pagination, rate-limit state, and JPEG downloads. It returns copied `NetworkEvent` values to the UI loop. The first playlist page is requested after authorization and retained by the UI, so opening the library normally requires no network round trip.

Now Playing is polled every two seconds while playing, every five seconds while paused, and every fifteen seconds without an active item. The displayed progress advances from the last observed Spotify timestamp between polls. Successful control commands cause a reconciliation poll after 400 ms.

## Memory

- LVGL uses two 240×40 RGB565 draw buffers, preferably in PSRAM with an internal-memory fallback.
- Current artwork is downloaded into PSRAM and decoded into an immutable RGB565 frame. A reference-counted handle crosses the network/UI queue so LVGL can never draw from a frame that the other core has freed or reused.
- Playback events are coalesced, limiting artwork ownership to the frame displayed by the UI and at most one pending replacement.
- Spotify API work closes any retained artwork connection before opening its TLS session. Row artwork is disabled on the current ESP32-S3 profile because repeated image handshakes fragment internal heap below the API handshake requirement; list symbols are used instead.
- Artwork is never written to flash or a filesystem.
- Playlist and track screens retain at most three 20-item pages.
- Liked Songs playback sends no more than 50 locally loaded URIs.

## Credentials

The desktop utility uses Spotify Authorization Code with PKCE. The authorization code, temporary access token, Wi-Fi password, and refresh token are never logged. The ESP32 stores Wi-Fi credentials, Client ID, and refresh token in versioned A/B NVS slots and writes the schema marker last before switching the active slot.

Access tokens exist only in RAM. If Spotify returns a rotated refresh token, it is written to the inactive NVS slot and verified before activation.

NVS is not encrypted in this hobby-device build. Someone with physical access and suitable hardware may extract stored credentials. Flash encryption and secure boot are deliberately excluded because provisioning them can irreversibly burn eFuses.

## Transport Security

All Spotify API, account, and artwork requests use `NetworkClientSecure` with the certificate bundle included by Arduino-ESP32. Certificate verification is never disabled. NTP synchronization must succeed before OAuth or API traffic begins.

An API `401` refreshes and retries once. A revoked or otherwise invalid refresh token switches the device directly back to USB provisioning mode. A `429` suspends requests for Spotify’s `Retry-After` interval. Transient failures use jittered 2, 4, 8, 16, 32, and 60-second delays. Offline touch commands are rejected visibly instead of being queued for surprising later execution.

## Spotify Boundaries

The requested scopes are limited to playback state/control, private and collaborative playlist reading, and saved-library reading. No email, profile-writing, playlist-writing, or streaming scope is requested.

Album artwork is displayed at its original square aspect ratio without cropping or overlays. Tapping the Spotify attribution displays a QR code to the item’s Spotify URL.
