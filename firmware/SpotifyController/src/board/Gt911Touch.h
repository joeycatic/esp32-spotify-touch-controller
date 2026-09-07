#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace spotctl {

class Gt911Touch {
public:
  bool begin(TwoWire &wire, uint16_t width, uint16_t height);
  bool read(uint16_t &x, uint16_t &y);

private:
  bool readRegisters(uint16_t reg, uint8_t *data, size_t length);
  bool writeRegister(uint16_t reg, uint8_t value);

  TwoWire *wire_{nullptr};
  uint16_t width_{0};
  uint16_t height_{0};
};

} // namespace spotctl
