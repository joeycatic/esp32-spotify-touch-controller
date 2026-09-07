#pragma once

#include <Arduino_GFX_Library.h>

#include "BoardBackend.h"
#include "Cst816Touch.h"

namespace spotctl {

class Compact2Backend final : public BoardBackend {
public:
  bool begin() override;
  bool touchReady() const override { return touch_ready_; }
  void draw(const lv_area_t &area, const lv_color_t *colors) override;
  bool readTouch(uint16_t &x, uint16_t &y) override;
  void setBacklight(uint8_t percent) override;
  const BoardCapabilities &capabilities() const override { return capabilities_; }

private:
  Arduino_ESP32SPI *bus_{nullptr};
  Arduino_ST7789 *display_{nullptr};
  Cst816Touch touch_;
  bool touch_ready_{false};
  const BoardCapabilities capabilities_{capabilitiesFor(BoardProfile::Compact2)};
};

} // namespace spotctl
