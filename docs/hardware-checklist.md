# Hardware Acceptance Checklist

Automated checks use `make test && make build`. Physical checks require Spotify Premium, 2.4 GHz Wi-Fi, and a Spotify Connect playback device. Record observations only on the named board; a previous compact upload is not post-refactor acceptance.

## Universal automated checks

- [x] Detector policy covers 7B, compact, partial, no-match, and ambiguous evidence.
- [x] Profile display/media/reset/serial constants and wide layout bounds are tested.
- [x] Host resolver covers Espressif USB, CH343 preference, pseudo-ports, overrides, and ambiguity.
- [x] Native C++ and Python provisioning suites pass.
- [x] One N16R8 firmware image compiles for both profiles.

## ESP32-S3-Touch-LCD-7B (SKU 31726)

2026-09-08 investigation: native C++ assertions and all 28 Python tests passed.
A 20 MHz pixel-clock trial compiled with pinned Arduino ESP32 3.3.11 and uploaded
on COM4 with flash hash verification. Although the reboot log confirmed panel
initialization, GT911, first LVGL flush, and backlight enable, the user reported a
black screen. Restoring 30 MHz restored the picture, confirmed by the user,
but whole-picture shifts/flicker remained. A second trial kept 30 MHz, used
twenty-line bounce buffers (+40 KiB internal RAM), and avoided unchanged label
and album-art redraws. It compiled, uploaded with hash verification, and logged
successful panel/touch initialization, but the user again reported a black screen.
The exact earlier 30 MHz/ten-line binary was restored from the local recovery
copy with hash verification, and all second-trial source changes were reverted
together. Which change caused the second black screen remains unconfirmed.
The failed trial's 30-second runtime diagnostic reported 127,196 bytes free internal RAM,
a 51,188-byte largest free block, and a 69,732-byte low-water mark. No API,
artwork, or crash errors appeared in the 45-second filtered startup capture;
these logs did not establish successful physical display or Spotify operation.

The user confirmed the second recovery restored the picture, with the original
flicker unresolved. Do not repeat either failed trial as a proposed fix.

Runtime configuration audit: the pinned Arduino 3.3.11 `qio_opi` SDK uses 80 MHz
PSRAM, 32-byte data-cache lines, size optimization, and automatic RGB restart
at VSYNC. PSRAM instruction/rodata placement is not enabled. This differs from
[Waveshare's 7B performance configuration](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B/Instructions-For-Use#performance-notes-esp-idf--lvgl),
which specifies 64-byte cache lines and PSRAM instruction/rodata placement.
[Espressif's RGB guidance](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html)
also requires 64-byte cache lines for bounce-buffer operation and identifies
memory contention as a drift cause. This is a documented configuration mismatch,
not proof of the physical root cause. Correcting SDK build options requires
rebuilding the underlying SDK/bootloader; editing sketch macros or the installed
`sdkconfig.h` alone does not rebuild precompiled Arduino libraries. Keep the
recovery firmware on the device while preparing that build. Any future comparison
must retain the working panel timings and isolate the change being tested.

2026-09-09 vendor baseline: saved all 16,777,216 flash bytes locally and verified
the backup against the device using `verify-flash` (digest matched); the saved
application also exactly matches the recovery binary. The ignored backup includes
private NVS setup and must not be committed or shared. A checksum-checking local
restore script is saved alongside it in `build/device-backup/restore.ps1`.

Flashed Waveshare's unmodified `firmware/13_lvgl_transplant.bin` at `0x0`, from
repository revision `c652c902db607f7ffb376257393cfd7657aa6428`, with write-hash
verification. This is the vendor's published binary, not a local build of the
current source. The embedded app identifies itself as `10_lvgl_transplant`,
built May 9, 2025 with ESP-IDF `v5.4-dev-3951-g9106c43acc-dirty`. Its startup
log confirms 120 MHz PSRAM, instruction/rodata placement in PSRAM, passing PSRAM
test, GT911 product-ID read, and LVGL demo startup. An I2C-address-initialization
warning preceded successful GT911 identification. The user reported a black
screen with this unmodified vendor binary. After a full power disconnect and
reconnect, the user reported the demo was present and "flawless". This is the
first confirmed stable vendor baseline. Warm-reset state must be controlled:
earlier black-screen trial results did not include the same cold-start check
and do not establish that those settings themselves caused the black screen.
The demo does not reproduce Spotify's Wi-Fi/TLS/artwork workload, so this result
does not isolate which software difference causes the original flicker.
The full Spotify backup was subsequently restored and independently verified
over all 16 MB (digest matched), including the original setup. The device is
back on the original Spotify firmware. The same full power cycle was requested
before evaluating Spotify flicker; the user confirmed it still flickers after
that comparison. Local image hashes and logs are in
`build/vendor-baseline/`.

2026-09-09 root cause. The panel runs in bounce-buffer mode, so
`lcd_rgb_panel_fill_bounce_buffer` copies 20,480 bytes out of the PSRAM
framebuffer inside the GDMA EOF ISR every ten scanlines. At 30 MHz with these
timings the deadline is about 462 us, and sustaining it costs roughly 40 MB/s of
PSRAM reads continuously. Measured flush throughput on hardware was 7.5 MB/s
with LVGL's draw buffer in PSRAM and 15 MB/s from internal RAM, so the bus is
running near saturation with the panel alone. When Wi-Fi, TLS and JPEG decode
compete, the ISR misses its deadline and the panel scans out stale lines. That
is the tearing, and it explains why it tracks Spotify activity.

The decisive comparison is the memory subsystem, not the panel:

| | PSRAM clock | Data cache line |
| --- | --- | --- |
| Arduino `qio_opi` SDK (precompiled) | 80 MHz | 32 bytes |
| Waveshare demo, flawless on this board | 120 MHz | 64 bytes |

Timings, bounce-buffer size, panel and GPIO map are identical between the two.
The vendor simply has 50% more PSRAM bandwidth and half the transaction count
for the ISR's bulk sequential reads. These settings are compiled into the
precompiled Arduino libraries and no sketch-level change reaches them, which is
what `firmware/idf` exists to work around. Compare
`build/vendor-baseline/published-sdkconfig.defaults` against
`.arduino/data/packages/esp32/tools/esp32s3-libs/3.3.11/qio_opi/include/sdkconfig.h`
before proposing any further display change.

Two application-level defects were found and fixed along the way. Neither is the
tearing cause, but both were wasting redraws:

- `Ui::tick` rewrote the elapsed-time label every loop pass. `lv_label_set_text`
  reallocates and invalidates unconditionally, so writing the identical string
  kept a region redrawing about thirty times a second while playing.
- The 7B uses one framebuffer with unsynchronized partial updates, so LVGL writes
  into the buffer while the panel scans it. **Superseded:** this entry used to
  claim the board had been moved to two framebuffers swapped at VSYNC. It had
  not. `num_fbs` stayed at 1 and the only thing added was a VSYNC semaphore that
  nothing ever waited on. See the 2026-09-09 correction below.

Black screens during this investigation were confounds, not results. Two causes
were identified and both are avoidable:

- `arduino-cli` incremental builds reuse `build/firmware`. After a struct layout
  change mid-session this produced inconsistent binaries. Always `rm -rf
  build/firmware` before a display comparison.
- A diagnostic that called `snprintf` and two blocking `Serial` writes from
  inside the LVGL flush callback, on the loop task stack beneath LVGL's render
  chain. Diagnostics must never run in the display hot path; the backend now
  has a `poll()` hook called from the main loop instead.

Because of these, single boots were repeatedly misread as evidence. Judge a
display change only after a full power disconnect, and never from one boot.

Do not enable `CONFIG_LCD_RGB_ISR_IRAM_SAFE` while the framebuffer is in PSRAM.
It looks like the obvious hardening and the driver does not reject the
combination, but `lcd_rgb_panel_fill_bounce_buffer` reads the PSRAM framebuffer
from the ISR, and the driver's own comment records that this crashes when the
cache is disabled. With an IRAM-safe ISR it would run during flash writes, so
token refresh and other NVS commits would crash intermittently.

The supplied [Tasmota discussion #22553](https://github.com/arendst/Tasmota/discussions/22553)
reports an incorrectly built Tasmota `libesp_lcd` for S3 octal PSRAM, fixed in
their February 2025 platform. A later January 2026 comment reports remaining
flicker on the exact 1024×600 7B. That Tasmota-specific fix is not proof that
our upstream Arduino 3.3.11 build has the same bug, and its 800×480 timings
must not be copied to the 7B. A separate Arduino-as-component build was
compiled under `firmware/idf`, retaining Arduino 3.3.11 / ESP-IDF 5.5.5 and
the application panel settings while rebuilding the framework with the
vendor memory/cache profile. It is not yet physically validated.

Source-built comparison flashed on 2026-09-09: application size 1,693,792 bytes,
46% free in the existing 3 MB slot; esptool image checksum and validation hash
passed. New bootloader, byte-identical partition table, and app0 were written
and independently verified against flash. NVS and OTA selection were preserved.
The boot log identifies `spotify_7b`, ESP-IDF 5.5.5, a passing PSRAM memory test,
the 120 MHz / 64-byte-cache / PSRAM-code profile, successful 7B and GT911
detection, RGB initialization, first LVGL flush, and backlight enable. Native
core assertions and all 28 Python tests pass. Image hashes/configuration are
recorded locally in `build/firmware-idf/comparison-manifest.json`; filtered
boot and flash logs are in `.tools/firmware-idf-boot.log` and
`.tools/firmware-idf-flash.log`. The user was asked to repeat the same complete
power disconnect before comparing idle and touch/scroll stability. That
physical result is pending; boot success is not a flicker-fix confirmation.

2026-09-09 correction, after re-reading the ESP-IDF 5.5.5 sources and the
generated `build/idf-sdkconfig` rather than the intended defaults.

**The source-built comparison never carried the vendor memory profile.** It set
`CONFIG_ESP32S3_DATA_CACHE_LINE_64B` and stopped there. The generated config
shows `CONFIG_ESP32S3_DATA_CACHE_32KB=y` and
`CONFIG_ESP32S3_INSTRUCTION_CACHE_16KB=y` -- ESP-IDF's defaults -- against
Waveshare's `CONFIG_ESP32S3_DATA_CACHE_64KB` and
`CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB`. Half the data cache is the wrong half
to be missing: the refill ISR streams sequentially through a 1.2 MB framebuffer
while Wi-Fi, TLS and JPEG decode evict its lines. The defaults now name all four
cache settings and `firmware/idf/main/sketch.cpp` asserts every one of them, so
a partially applied profile fails the build instead of shipping.

`CONFIG_SPIRAM_XIP_FROM_PSRAM` does not exist for the ESP32-S3 in 5.5.5 (only
C5/C61/P4). `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` + `CONFIG_SPIRAM_RODATA` are the
S3 spelling and were already correct.

**120 MHz octal PSRAM runs without temperature tracking, and must keep doing so
on this board.** ESP-IDF's Kconfig help states that at this speed accesses
"crash randomly" once the die drifts about 20 C from power-on, and gates the
remedy behind `CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR`.
Enabling it was tried on 2026-09-09 and **boot-looped the device**: 16 reboots in
15 seconds, each one
`E MSPI Timing: The flash model has not been verified support this feature`
followed by a backtrace and `rst:0xc (RTC_SW_CPU_RST)`. The cause is in
`components/esp_hw_support/mspi_timing_tuning/port/esp32s3/mspi_timing_by_mspi_delay.c`:
`psram_adjust_timing_point_via_tsens()` starts its retuning task only for flash
vendor ID `0xC8` or `0x20` and returns `ESP_ERR_NOT_SUPPORTED` otherwise, from an
`ESP_SYSTEM_INIT_FN` -- so startup aborts before any application code runs. This
board's flash is neither vendor. The option is now rejected by a compile-time
`#error` so it cannot be re-added by the same reasoning.

Waveshare's defaults set only the measurement interval, which is inert without
that switch. So the vendor demo that runs flawlessly on this board runs 120 MHz
PSRAM with a boot-time tuning point and no retuning, and that is the
configuration to match. The temperature risk is real and stays open: if
long-run instability appears that a cold start clears, the available remedy is
`CONFIG_SPIRAM_SPEED_80M`, not this switch.

**Two application defects were cancelling the fix they claimed to implement.**

- `Board::allocateDrawBuffer` requested `MALLOC_CAP_SPIRAM` first and fell back
  to internal RAM, under a comment explaining at length why the render target
  must not be in PSRAM. Every boot took the PSRAM path, so the measured
  15 MB/s -> 7.5 MB/s penalty was in effect the whole time and the checklist item
  below could never have read `internal RAM`. Internal is now tried first.
- The 7B profile asked for 40 rows x 2 buffers = 160 KiB of draw buffer. That
  cannot fit internal RAM beside the panel, so it guaranteed the PSRAM fallback
  above. Restored to the intended 16 rows x 1 buffer = 32 KiB. A second buffer
  buys nothing against a synchronous flush.

Bounce buffers are back to Waveshare's ten scanlines. Thirty lines cost 80 KiB
of internal RAM that the draw buffer and the larger caches need, and the refill
deadline was never the binding constraint -- PSRAM read bandwidth is.

**The black screens were the board, not the changes.** Waveshare's own
[FAQ](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7B/FAQ) answers "the program
flashes successfully, but nothing is shown on the screen" with: press reset, or
disconnect and reconnect power. That is the same artifact this investigation saw
on the unmodified vendor binary, and every flash ends in a warm reset. The
expander has its own power and latches its outputs, so on a warm reset the panel
rail is already enabled and the panel keeps whatever state it held when the CPU
was reset; only pulsing `LCD_RST` does not clear it. `Wide7BBackend::begin` now
drops and restores the panel rail before releasing reset, so a warm reset reaches
the panel as a cold start. Judge this by whether black screens stop needing a
physical power cycle.

This reframes the record above: the 20-line and 30-line bounce-buffer trials
were each judged by a single post-flash boot, which is exactly the condition
Waveshare says produces a black screen. Neither trial was shown to cause it.

The dead VSYNC scaffolding is gone. `vsync_semaphore_` was given by an ISR and
never taken by anything, `framebuffers()` returned false, and `draw()` carried a
comment claiming the call was "a buffer swap rather than a copy" -- with
`num_fbs = 1` it is a copy into the live framebuffer. Code that claims to do the
thing under investigation, while doing nothing, is why this took as long as it
did. What a second framebuffer would really buy is recorded in `draw()`:
`lcd_rgb_panel_fill_bounce_buffer` latches `bb_fb_index = cur_fb_index` only when
`bounce_pos_px` wraps, so bounce mode does give a genuine frame-boundary swap --
but reaching it means LVGL renders full frames into PSRAM, adding ~1.2 MB of
PSRAM writes per frame to the bus whose saturation is the actual defect. Not
until the memory profile has headroom to spare.

2026-09-09 first confirmed physical improvement. After the corrected build,
the user reports the display working. Measured on the device over 25 seconds,
idle, with no resets in the window:

| | Before | After |
| --- | --- | --- |
| `[7B] flush ... KB/s` | 14,033 KB/s | 54,065 KB/s |
| Time in flush per 5 s window | 83 ms | 22 ms |

The throughput figure alone would be suggestive; the 83 ms -> 22 ms is the one
that matters, because it is time the loop task spends holding the PSRAM bus
against the refill ISR's deadline. 54 MB/s is also not reachable by a
PSRAM-to-PSRAM copy, which independently confirms the LVGL draw buffer is now in
internal RAM -- the fix that was written months ago, inverted in code, and never
once actually in effect.

Attribution is not established. Four things changed together: draw buffer to
internal RAM, 16 rows x 1 buffer, bounce buffers back to ten lines, and the
64 KB data cache / 32 KB instruction cache. Any subset may account for the
result. Do not cite this entry as evidence for any single one of them.

Nor is the original black screen explained. When serial was first captured this
session the board was alive and repainting about a full screen every five
seconds while displaying nothing -- not a crash, not a boot loop. Whether the
panel-rail power cycle fixed that, or one of the memory changes did, or it was
the full power disconnect, is unknown. The continuous idle repaint is still
unexplained and is the best remaining lead.

Still unmeasured: behaviour under load. The flicker was always reported while
Spotify was doing work. Repeat the flush comparison with playback running,
artwork loading, and Library scrolling before treating the tearing as resolved.

2026-09-09, two-framebuffer attempt: **failed, reverted.** Recorded so it is not
retried on the same reasoning.

The residual artifact is a narrow distorted band at the left edge, intermittent,
under load. That is the signature of a FIFO underrun: the peripheral clocks a
line out late, and the seam lands at the start of the line. It is the original
whole-picture shift, much reduced.

Waveshare's `published-rgb_lcd_port.c` was compared against ours directly. Every
porch, the sync polarity, the bounce-buffer size and the GPIO map match exactly.
`dma_burst_size` is a non-difference: the driver substitutes 64 when the field is
zero, which is what we set. Two real deltas remained: `pclk_hz` 30.85 MHz vs our
30, and `num_fbs` 2 vs our 1.

Both were changed at once, which isolated nothing and wasted a flash cycle. The
result was a black screen. Reverting only the pixel clock to 30 MHz, leaving
`num_fbs = 2`, was still black. Reverted to `num_fbs = 1` at 30 MHz.

**Correction, same day: that conclusion does not hold.** It was recorded as
"`num_fbs = 2` blacks this panel". Both of those trials ran inside a window in
which the *reverted* `num_fbs = 1` firmware also went from a working picture to
a washed-out panel to no picture at all, on byte-identical source. Whatever was
making the display intermittent during that window is unexplained, and it means
neither black screen can be attributed to `num_fbs = 2`. Treat two framebuffers
as untested, not as ruled out. Waveshare's flawless demo uses `num_fbs = 2`.

What makes this worth recording is that every diagnostic reported success:

```
[7B] RGB timing: 30 MHz pixel clock, 10-line bounce buffers, 2 framebuffers
[7B] both framebuffers cleared; panel ready
[board] LVGL renders into the panel framebuffers, swapped at the frame boundary
[7B] first LVGL framebuffer flush complete
[7B] backlight on
```

No allocation failure, no fallback, no crash, no reset. Flush time fell to 0 ms
for a full 1200 KB area, which confirms `esp_lcd_panel_draw_bitmap` took its
no-copy path and recorded `cur_fb_index` as intended. The swap was submitted and
the panel still showed nothing. Why the frame-boundary latch does not reach the
glass on this board is unexplained; the ESP-IDF mechanism
(`lcd_rgb_panel_fill_bounce_buffer` moving `bb_fb_index` to `cur_fb_index` on
wrap) reads as though it should work, and on this hardware it does not.

Do not retry this on the argument that Waveshare's demo does it. That inference
failed twice today: first with
`CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR`, which boot-looped the
board because the feature is gated on flash vendor ID, and then here. The vendor
demo runs a different SDK configuration with no Wi-Fi and no TLS. "The vendor
does X" is evidence, not proof.

If the left-edge band is attacked again, the untried lever is *lowering* the
pixel clock, which lengthens the per-line refill deadline. That is the opposite
direction from the vendor-parity change tried here, and it must be the only
variable in its test.

2026-09-09 hardware cleared, cause isolated. Waveshare's unmodified
`13_lvgl_transplant.bin` was flashed and reported flawless. The panel, its
ribbon and the board are therefore all sound, and every black screen recorded
above was software or transient -- not damage. The full flash was restored from
the verified backup afterwards.

With the application firmware back, the reported symptom is horizontal
flickering stripes and general glitching **that appears once Spotify is in
use**. This is the cleanest comparison available and it isolates the cause:

| | Wi-Fi | TLS | JPEG decode | Result |
| --- | --- | --- | --- | --- |
| Waveshare demo | no | no | no | flawless |
| This firmware | yes | yes | yes | stripes under load |

Panel timings, bounce-buffer size, GPIO map and the memory profile are otherwise
equivalent. Horizontal stripes are the bounce-buffer refill ISR missing its
deadline and the panel scanning out stale lines. The defect is contention for
PSRAM between the panel's continuous read stream and the application's network
and decode work -- not panel configuration.

The panel demands a *continuous* PSRAM read stream, and the pixel clock sets it:

| pclk | refresh | PSRAM reads | 10-line deadline |
| --- | --- | --- | --- |
| 30 MHz | 32.7 Hz | 40.2 MB/s | 462 us |
| 25 MHz | 27.3 Hz | 33.5 MB/s | 554 us |
| 22 MHz | 24.0 Hz | 29.5 MB/s | 630 us |

Lowering it would reduce demand and lengthen the deadline at the same time, at a
refresh rate that does not matter for this application.

**This does not work. The pixel clock is a hardware floor, not a tuning knob.**
25 MHz was tried as a clean single variable against a 30 MHz build that had
produced a picture, and the panel went black. That is now the second data point:
20 MHz also blacked it. 30 MHz is the only clock that has ever displayed
anything on this board, and the vendor uses 30.85 MHz. This is what a panel
timing controller does below its minimum specified pixel clock. The earlier
20 MHz result had been dismissed as a warm-reset confound; the 25 MHz test shows
it was real.

Restored to 30 MHz. **Do not spend another cycle lowering the pixel clock.**
The panel's ~40 MB/s continuous PSRAM read demand is fixed by hardware, so
relieving contention has to come from the application side. Untried levers:

- The JPEG decode path's PSRAM traffic, which matches the reported correlation
  between stripes and artwork loading. Decoding into internal RAM, or bounding
  concurrent decode work, would cut the largest burst competing with the panel.
- `num_fbs = 2`, which is untested rather than ruled out (see the correction
  above) and is what Waveshare's flawless demo uses.

What this round does **not** establish:

- The panel-rail power cycle is reasoned from the expander's documented role
  (the design spec lists expander control of "touch reset, LCD reset, power,
  USB/CAN selection, and backlight") plus Waveshare's FAQ remedy. It is not
  confirmed against a schematic. If black screens persist unchanged after a warm
  reset, the pin is not the panel rail and this change should be reverted rather
  than built on.
- Whether the corrected cache profile is sufficient on its own. It closes the
  measured gap to the flawless vendor baseline, but the vendor demo does not run
  Wi-Fi, TLS or JPEG decode, so its headroom is not proof of ours.
- Whether die-temperature drift contributes at all. The one remedy ESP-IDF
  offers is unavailable on this board's flash (see above), so this stays
  untested. The flicker was reported from cold as well, so it cannot be the
  whole cause.
- **The panel-rail power cycle has still never executed.** The build carrying it
  boot-looped for an unrelated reason, so the board never reached display
  initialization. Nothing about that hypothesis has been tested yet.

Method, for the next person: this board shows a black screen after a warm reset
often enough that a single post-flash boot proves nothing. Power-cycle fully,
then judge. Record the `[board] LVGL draw buffer: ... internal RAM` line and the
five-second `[7B] flush ... KB/s` figure on every run -- a change that does not
move those numbers did not change what it claimed to change.

### Bring-up and provisioning

- [x] UART1 / USB TO UART enumerates through the onboard CH343 and `make flash` selects it.
- [x] Upload and flash verification complete without manually specifying `PORT`.
- [x] Cold boot and a remembered-profile reboot both report `Wide7B`.
- [ ] Display is 1024×600 landscape with correct colors, no drift/flicker/tearing, and stable backlight.
- [ ] Serial boot log reports `RGB timing: 30 MHz pixel clock, 10-line bounce buffers`. Check the setup screen before Wi-Fi, then leave Now Playing untouched for five minutes with Spotify connected; neither should develop shifted lines or flicker.
- [ ] Boot log reports `[board] LVGL draw buffer: 1024 x 16 rows, 1 buffer(s), internal RAM`. `PSRAM (degraded)` means the internal allocation failed and the throughput fix is not in effect.
- [ ] While artwork/thumbnails load, repeatedly drag volume/seek, hold a control, and scroll Library. Watch specifically for a narrow distorted band at the left edge: that is a FIFO underrun clocking a line out late, and it is the residual form of the original whole-picture shift. It is a known, unresolved limitation; see the two-framebuffer result below.
- [ ] Compare the five-second `[7B] flush ... KB/s copied into PSRAM` line on an idle Now Playing screen against the moment a cover loads. A large drop is PSRAM bus contention; it should no longer coincide with visible tearing.
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
