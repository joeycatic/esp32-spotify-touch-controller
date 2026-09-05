# ESP32 Spotify Touch Controller Implementation Plan

Build a standalone Spotify display and remote for the Waveshare ESP32-S3-Touch-LCD-2 using Arduino, LVGL, direct Spotify Web API access, and a one-time Python PKCE provisioning utility.

The implementation is divided into project/toolchain setup, testable core state and policies, secure provisioning, board support, Spotify networking, the cover-first touch interface, verification, and user documentation. The authoritative requirements are the plan approved in the project conversation on 2026-09-05.

Implementation note: Arduino_GFX 1.5.0 from the original plan does not compile with Arduino-ESP32 3.3.11 because Espressif changed `spiFrequencyToClockDiv` in core 3.3.6. The project therefore pins Arduino_GFX 1.6.7, which contains the upstream compatibility fix.
