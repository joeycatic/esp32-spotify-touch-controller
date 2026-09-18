#pragma once

#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <lvgl.h>

#include "BoardProfile.h"
#include "Gt911Touch.h"
#include "WaveshareIoExpander7B.h"

namespace spotctl {

// The one display backend. This was a virtual BoardBackend while a 240x320
// board shared the code; with a single target the indirection only hid which
// panel the caller was really talking to.
class Wide7BBackend {
public:
  bool begin();
  bool touchReady() const { return touch_ready_; }
  void draw(const lv_area_t &area, const lv_color_t *colors);
  bool readTouch(uint16_t &x, uint16_t &y);
  void setBacklight(uint8_t percent);
  // Called from the main loop, outside LVGL's render and flush stack, so
  // diagnostics never add work to the display hot path.
  void poll();

private:
  void reportFlushThroughput();

  esp_lcd_panel_handle_t panel_{nullptr};
  void *frame_buffer_{nullptr};
  WaveshareIoExpander7B expander_;
  Gt911Touch touch_;
  bool touch_ready_{false};
  bool first_draw_logged_{false};
  uint32_t flush_us_{0};
  uint32_t flush_bytes_{0};
  uint32_t flush_count_{0};
  uint32_t report_started_ms_{0};
};

} // namespace spotctl
