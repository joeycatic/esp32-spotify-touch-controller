#pragma once

#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#include "BoardBackend.h"
#include "Gt911Touch.h"
#include "WaveshareIoExpander7B.h"

namespace spotctl {

class Wide7BBackend final : public BoardBackend {
public:
  bool begin() override;
  bool touchReady() const override { return touch_ready_; }
  void draw(const lv_area_t &area, const lv_color_t *colors) override;
  bool readTouch(uint16_t &x, uint16_t &y) override;
  void setBacklight(uint8_t percent) override;
  void poll() override;
  const BoardCapabilities &capabilities() const override { return capabilities_; }

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
  const BoardCapabilities capabilities_{capabilitiesFor(BoardProfile::Wide7B)};
};

} // namespace spotctl
