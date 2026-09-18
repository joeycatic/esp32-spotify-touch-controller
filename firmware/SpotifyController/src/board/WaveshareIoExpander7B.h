#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace spotctl {

class WaveshareIoExpander7B {
public:
  enum Pin : uint8_t {
    TouchReset = 1,
    BacklightEnable = 2,
    LcdReset = 3,
    SdChipSelect = 4,
    UsbCanSelect = 5,
    LcdVcomEnable = 6,
  };

  // The panel performs its own power-on initialization. Keep its reset and
  // VCOM controls asserted across ESP32 resets; only touch reset and the
  // backlight start low. Board detection and backend startup must use the same
  // mask so neither stage can silently reset an already powered panel.
  static constexpr uint8_t startupOutputs(bool touch_released = false) {
    return static_cast<uint8_t>(0xD9U |
                                (touch_released ? (1U << TouchReset) : 0U));
  }

  bool begin(TwoWire &wire);
  // Drive one LCD reset edge, leaving VCOM and every other output untouched.
  bool pulseLcdReset();
  bool set(Pin pin, bool high);
  bool setBacklight(uint8_t percent);

private:
  bool writeRegister(uint8_t reg, uint8_t value);

  TwoWire *wire_{nullptr};
  uint8_t output_{0};
};

} // namespace spotctl
