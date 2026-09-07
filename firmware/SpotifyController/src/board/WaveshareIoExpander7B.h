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
    LcdPowerEnable = 6,
  };

  bool begin(TwoWire &wire);
  bool set(Pin pin, bool high);
  bool setBacklight(uint8_t percent);

private:
  bool writeRegister(uint8_t reg, uint8_t value);

  TwoWire *wire_{nullptr};
  uint8_t output_{0};
};

} // namespace spotctl
