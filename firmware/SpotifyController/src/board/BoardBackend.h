#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "BoardProfile.h"

namespace spotctl {

class BoardBackend {
public:
  virtual ~BoardBackend() = default;
  virtual bool begin() = 0;
  virtual bool touchReady() const = 0;
  virtual void draw(const lv_area_t &area, const lv_color_t *colors) = 0;
  virtual bool readTouch(uint16_t &x, uint16_t &y) = 0;
  virtual void setBacklight(uint8_t percent) = 0;
  virtual const BoardCapabilities &capabilities() const = 0;
};

} // namespace spotctl
