# Universal Board Profiles Design

**Date:** 2026-09-06
**Status:** Approved for implementation planning

## Summary

The firmware will become one universal ESP32-S3 binary supporting two Waveshare boards:

- ESP32-S3-Touch-LCD-2 with a 240×320 SPI ST7789 display and CST816 touch controller
- ESP32-S3-Touch-LCD-7B, SKU 31726, with a 1024×600 RGB ST7262 display, GT911 touch controller, and CH422G I/O expander

The binary will detect the attached board before initializing any display bus, select a board backend for the entire boot session, and choose a native UI layout for that profile. The existing 2-inch experience will remain compact and portrait-oriented. The 7B will use the approved focused-split landscape layout.

The Spotify client, application state, provisioning protocol, storage, networking, and playback command behavior remain shared.

## Goals

- Produce one firmware binary that boots correctly on either supported board without a build-time model choice.
- Preserve the physically validated 2-inch display, touch, and UI behavior.
- Provide a native 1024×600 landscape interface on the 7B rather than scaling the portrait UI.
- Detect hardware conservatively and never guess when detection is inconclusive.
- Keep model-specific display, touch, backlight, and recovery behavior behind a small board interface.
- Maintain the existing 16 MB flash and 8 MB OPI PSRAM build target shared by both boards.

## Non-goals

- Generic support for every ESP32 display board
- Runtime resolution changes after startup
- Hot-swapping display hardware without rebooting
- Support for the non-touch LCD-7B variant
- Redesigning Spotify API behavior or provisioning credentials
- Adding unrelated 7B peripherals such as CAN, RS485, battery monitoring, or SD-card support

## Hardware Identification

### Detection sequence

Detection runs before LVGL, SPI display, RGB display, backlight, or display-owned GPIO initialization.

1. Load the last confirmed model from a dedicated NVS key.
2. Verify the remembered model using its non-display I²C signature.
3. If verification fails or no model is stored, probe the 7B hardware on GPIO 8/9.
4. If the 7B probe does not confirm the board, release/reconfigure I²C and probe the 2-inch CST816 on GPIO 48/47.
5. Save a newly confirmed model and use it for the rest of the boot session.

The 7B probe must verify the GT911 product identity and the expected CH422G control path. The 2-inch probe must verify the CST816 chip ID already used by the current driver. An address acknowledgment alone is not sufficient.

The saved model is an optimization, not an authority. Every boot verifies it before configuring display pins. This permits recovery from stale or corrupted NVS and supports moving a flashed ESP32 module between compatible assemblies.

### Inconclusive detection

If neither signature is confirmed, the firmware must not initialize either display backend. It will emit sanitized USB serial diagnostics containing:

- each profile attempted;
- I²C pins and device class, but no credentials;
- whether the bus, address, and identity checks succeeded;
- the final `unsupported_or_unresponsive_hardware` result.

The detector must not fall back to a default model.

## Board Architecture

### Common interface

`Board` remains the application-facing owner and delegates hardware work to one selected `BoardBackend`. The interface exposes only what the rest of the application needs:

- initialize hardware and LVGL display/input drivers;
- tick or service any backend work;
- report logical width and height;
- flush a rendered LVGL area;
- read one primary touch point;
- set backlight brightness where supported;
- report display, touch, flash, and PSRAM status;
- report boot-time recovery intent.

The selected backend is immutable until reboot. Application and UI code never inspect controller types or board GPIOs.

### Compact 2-inch backend

The compact backend retains the existing, physically tested behavior:

- 240×320 portrait resolution;
- Arduino_GFX SPI/ST7789 display path;
- CST816 I²C touch on GPIO 48/47;
- GPIO1 PWM backlight;
- GPIO0 BOOT-button recovery behavior;
- two 240×40 RGB565 LVGL draw buffers, preferring PSRAM.

Refactoring this path must be mechanical. Display timings, orientation, touch transformation, pin assignments, and UI coordinates do not change unless a regression test demonstrates that a correction is required.

### Wide 7B backend

The wide backend provides:

- 1024×600 landscape resolution;
- ST7262 over the ESP32-S3 RGB LCD peripheral;
- GT911 touch on the shared GPIO8/GPIO9 I²C bus;
- CH422G initialization for touch reset, LCD reset, and backlight control;
- board-specific RGB timing and pin mapping taken from the official Waveshare 7B example;
- PSRAM-backed framebuffer and bounded LVGL draw buffers.

Initialization order is significant: shared I²C, CH422G safe output state, GT911 reset/address selection, LCD reset, RGB panel, first black frame, and finally backlight enable. Partial initialization must leave the backlight off where possible.

The official hardware references are:

- <https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B>
- <https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B/FAQ>
- <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7B>

## UI Architecture

The UI keeps one shared controller for state, network events, commands, artwork handles, navigation, and screen lifetime. Geometry and widget construction are separated into two layout implementations:

- `CompactLayout` builds the existing 240×320 screens.
- `WideLayout` builds native 1024×600 screens.

This split avoids scattering resolution checks throughout individual widget coordinates. Shared callbacks operate on semantic widget roles rather than assuming where those widgets are positioned.

### Wide now-playing layout

The approved 7B layout is the focused split:

- slim top bar with Spotify attribution on the left and device/connectivity state on the right;
- large square cover artwork in the left pane;
- title, artist, progress, elapsed/duration, playback controls, shuffle, and repeat in the right pane;
- persistent bottom navigation for Now Playing, Library, Devices, Volume, and QR;
- library, playlist, device picker, volume, diagnostics, setup, and QR remain separate full screens or overlays.

The wide layout uses the extra space for larger touch targets and readable metadata, not for new Spotify features. It must preserve all controls and states available on the compact layout.

### Artwork sizing

Artwork dimensions become profile-driven:

- compact player art remains 184 px;
- wide player art is decoded and displayed at 400 px;
- row thumbnails are sized per layout while retaining the current bounded cache and immutable frame ownership.

Artwork decode limits must be derived from the requested target size rather than global 2-inch constants.

## Startup and Data Flow

The startup flow becomes:

1. Start sanitized USB serial output.
2. Detect and select the board profile.
3. Capture boot-time recovery intent before display-owned GPIOs are configured.
4. Initialize the selected backend and its LVGL drivers.
5. Select the compact or wide layout.
6. Continue the existing provisioning or Wi-Fi/Spotify startup path.

After selection, playback and library events continue to cross the existing network/UI queue. The layout renders shared models and produces the same `UiCommand` values. No board-specific data enters Spotify or networking classes.

## Provisioning and Factory Reset

The 2-inch board retains its existing recovery behavior, including the visible ten-second BOOT hold countdown.

On the 7B, GPIO0 participates in the RGB display bus, so the firmware must not continuously treat it as an input after panel initialization. The backend samples BOOT before RGB startup:

- BOOT held during reset enters provisioning mode.
- Factory reset is exposed through an on-screen `Reset setup` action with a deliberate hold-to-confirm interaction.
- USB provisioning remains available when the display or touch cannot complete normal startup.

The reset action erases the same Wi-Fi and Spotify configuration as the current factory reset. The remembered board model may remain because it is not a credential and is verified at every boot; erasing it is also safe because detection will rerun.

## Memory and Performance

The 7B RGB565 framebuffer requires approximately 1.23 MB. Two 1024×40 RGB565 LVGL draw buffers require approximately 160 KB. A 400×400 RGB565 artwork frame requires approximately 320 KB. These allocations fit within 8 MB PSRAM alongside the bounded JPEG, thumbnail, LVGL, networking, and application allocations.

The wide backend must allocate large display memory explicitly from PSRAM and fail clearly if the required framebuffer cannot be allocated. It must not fall back to internal SRAM for the 1024×600 framebuffer. Buffer row count and RGB pixel clock may be tuned during hardware validation without changing the interface.

The compact backend keeps its current internal-memory fallback for small draw buffers.

## Failure Handling

- **Remembered profile verification fails:** perform full detection.
- **No model is confirmed:** initialize no display backend and report serial diagnostics.
- **Display initialization fails:** stop that hardware path safely; never attempt the other backend after conflicting pins may have been configured.
- **Touch initialization fails:** keep the display active, show a hardware diagnostic, and retain USB recovery.
- **Required 7B PSRAM allocation fails:** keep the backlight off, report the allocation failure, and do not start the UI.
- **Unknown future model:** report unsupported hardware rather than choosing the closest resolution.
- **Network or Spotify failure:** preserve existing offline, authorization, retry, and rate-limit behavior independent of board profile.

## Verification

### Automated tests

- Detector selects the 7B only when its expected identity checks pass.
- Detector selects the 2-inch profile only when the CST816 identity check passes.
- Remembered profiles are verified, invalidated on mismatch, and replaced after successful detection.
- No-match and ambiguous responses produce an unsupported result rather than a default.
- Touch transformations stay within 240×320 and 1024×600 bounds for all tested corners.
- Layout geometry keeps required widgets within each display boundary.
- Both layouts produce the same semantic playback and navigation commands.
- Artwork target selection and memory calculations are profile-driven and bounded.
- Native C++ and Python suites continue to pass.
- The Arduino toolchain produces one universal firmware image using the existing N16R8 FQBN.

Hardware drivers that cannot run natively will be isolated behind small interfaces so detector and layout policy can use fakes in native tests.

### Physical acceptance

The previously successful 2-inch test becomes the regression baseline. It must repeat display, touch corners, playback controls, library navigation, provisioning entry, and factory reset after the refactor.

The 7B acceptance run must verify:

- correct 1024×600 orientation, colors, timing, and stable backlight;
- accurate GT911 corner and gesture coordinates;
- every focused-split screen and overlay;
- artwork appearance and row-thumbnail loading;
- Spotify playback, seeking, volume, shuffle, repeat, library, and device transfer;
- BOOT-at-reset provisioning entry and on-screen factory reset;
- Wi-Fi recovery, token refresh, rate limiting, and one-hour soak behavior;
- no display drift, reboot, frozen touch, or unbounded memory loss.

## Documentation Changes

Implementation will update the README, setup guide, architecture notes, and hardware checklist to:

- list both supported models and identify the universal binary;
- mark the 2-inch board as physically tested;
- add 7B flashing and acceptance instructions;
- explain automatic detection and serial diagnostics;
- document the different factory-reset interactions.

## Success Criteria

The work is complete when the same built firmware image boots on both boards, selects the correct backend without user configuration, preserves the tested 2-inch behavior, presents the native focused-split interface on the 7B, passes automated verification, and completes the relevant physical acceptance checklist on both devices.
