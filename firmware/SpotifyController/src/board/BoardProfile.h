#pragma once

#include <cstddef>
#include <cstdint>

namespace spotctl {

struct DisplayMetrics {
  uint16_t width;
  uint16_t height;
  uint16_t draw_buffer_rows;
  // A second draw buffer buys nothing while the backend flush is a synchronous
  // copy: LVGL cannot render ahead of a blocking flush. The panel renders
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
  const char *model;
  DisplayMetrics display;
  MediaPolicy media;
};

// The only supported board. This firmware targets the 7B alone: its 1024x600
// RGB panel, its CH422G expander and its UART1 console are assumed throughout,
// and nothing detects or adapts to anything else.
inline constexpr BoardCapabilities kBoardCapabilities{
    "Waveshare ESP32-S3-Touch-LCD-7B",
    {1024, 600, 16, 1},
    {400, 64, true},
};

constexpr size_t frameBufferBytes(const DisplayMetrics &display) {
  return static_cast<size_t>(display.width) * display.height * sizeof(uint16_t);
}

constexpr size_t drawBufferBytes(const DisplayMetrics &display) {
  return static_cast<size_t>(display.width) * display.draw_buffer_rows *
         sizeof(uint16_t) * display.draw_buffer_count;
}

} // namespace spotctl
