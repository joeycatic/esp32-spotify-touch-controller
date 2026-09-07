#pragma once

#include <cstdint>

namespace spotctl {

inline bool transformTouchPoint(uint16_t raw_x, uint16_t raw_y,
                                uint8_t rotation, uint16_t width,
                                uint16_t height, uint16_t &x, uint16_t &y) {
  switch (rotation) {
  case 1:
    x = raw_y;
    y = static_cast<uint16_t>(height - 1 - raw_x);
    break;
  case 2:
    x = static_cast<uint16_t>(width - 1 - raw_x);
    y = static_cast<uint16_t>(height - 1 - raw_y);
    break;
  case 3:
    x = static_cast<uint16_t>(width - 1 - raw_y);
    y = raw_x;
    break;
  default:
    x = raw_x;
    y = raw_y;
    break;
  }
  return x < width && y < height;
}

} // namespace spotctl
