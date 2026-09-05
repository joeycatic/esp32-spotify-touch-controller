#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace spotctl {

class Cst816Touch {
public:
  bool begin(TwoWire &wire, uint8_t rotation, uint16_t width, uint16_t height);
  bool read(uint16_t &x, uint16_t &y);
  bool detected() const { return detected_; }

private:
  bool readRegisters(uint8_t reg, uint8_t *data, size_t length);
  void transform(uint16_t raw_x, uint16_t raw_y, uint16_t &x,
                 uint16_t &y) const;

  TwoWire *wire_{nullptr};
  uint8_t rotation_{0};
  uint16_t width_{240};
  uint16_t height_{320};
  bool detected_{false};
};

} // namespace spotctl

