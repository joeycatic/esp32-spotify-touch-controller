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
  // Called from the main loop, outside LVGL's render and flush stack, so a
  // backend can do diagnostics without adding work to the display hot path.
  virtual void poll() {}
  // A backend that scans out of its own framebuffers lets LVGL render straight
  // into them and swaps at VSYNC, which is what keeps the panel from showing a
  // half-updated frame. Backends without that capability return false and get
  // a separately allocated draw buffer instead.
  virtual bool framebuffers(void *&first, void *&second) {
    first = nullptr;
    second = nullptr;
    return false;
  }
  virtual const BoardCapabilities &capabilities() const = 0;
};

} // namespace spotctl
