#pragma once

#include <cstddef>
#include <cstdint>

namespace spotctl {

enum class BoardProfile : uint8_t { Unknown = 0, Compact2 = 1, Wide7B = 2 };

enum class SerialTransport : uint8_t { NativeUsb, Uart0 };
enum class FactoryResetPolicy : uint8_t { BootHold10Seconds, OnScreenHold3Seconds };

struct DisplayMetrics {
  uint16_t width;
  uint16_t height;
  uint16_t draw_buffer_rows;
  // A second draw buffer buys nothing while the backend flush is a synchronous
  // copy: LVGL cannot render ahead of a blocking flush. The RGB board renders
  // single-buffered so its one buffer fits in internal RAM, which is what keeps
  // the flush off the PSRAM bus the panel's refill ISR is already saturating.
  uint8_t draw_buffer_count;
};

struct MediaPolicy {
  uint16_t player_art_size;
  uint16_t thumbnail_size;
  bool row_thumbnails;
};

struct BoardCapabilities {
  BoardProfile profile;
  const char *model;
  DisplayMetrics display;
  MediaPolicy media;
  FactoryResetPolicy factory_reset;
  SerialTransport serial;
};

constexpr BoardCapabilities capabilitiesFor(BoardProfile profile) {
  return profile == BoardProfile::Wide7B
             ? BoardCapabilities{BoardProfile::Wide7B,
                                 "Waveshare ESP32-S3-Touch-LCD-7B",
                                 {1024, 600, 16, 1},
                                 {400, 64, true},
                                 FactoryResetPolicy::OnScreenHold3Seconds,
                                 SerialTransport::Uart0}
         : profile == BoardProfile::Compact2
             ? BoardCapabilities{BoardProfile::Compact2,
                                 "Waveshare ESP32-S3-Touch-LCD-2",
                                 {240, 320, 40, 2},
                                 {184, 40, false},
                                 FactoryResetPolicy::BootHold10Seconds,
                                 SerialTransport::NativeUsb}
             : BoardCapabilities{BoardProfile::Unknown,
                                 "Unknown ESP32-S3 board",
                                 {0, 0, 0, 1},
                                 {0, 0, false},
                                 FactoryResetPolicy::BootHold10Seconds,
                                 SerialTransport::NativeUsb};
}

struct DetectionEvidence {
  bool wide_expander{false};
  bool wide_gt911{false};
  bool compact_cst816{false};
};

constexpr BoardProfile selectDetectedProfile(const DetectionEvidence &evidence) {
  const bool wide = evidence.wide_expander && evidence.wide_gt911;
  if (wide == evidence.compact_cst816) {
    return BoardProfile::Unknown;
  }
  return wide ? BoardProfile::Wide7B : BoardProfile::Compact2;
}

constexpr BoardProfile selectBoardProfile(BoardProfile remembered,
                                          bool remembered_verified,
                                          const DetectionEvidence &evidence) {
  return remembered != BoardProfile::Unknown && remembered_verified
             ? remembered
             : selectDetectedProfile(evidence);
}

inline const char *boardProfileName(BoardProfile profile) {
  return capabilitiesFor(profile).model;
}

constexpr size_t frameBufferBytes(const DisplayMetrics &display) {
  return static_cast<size_t>(display.width) * display.height * sizeof(uint16_t);
}

constexpr size_t drawBufferBytes(const DisplayMetrics &display) {
  return static_cast<size_t>(display.width) * display.draw_buffer_rows *
         sizeof(uint16_t) * display.draw_buffer_count;
}

} // namespace spotctl
