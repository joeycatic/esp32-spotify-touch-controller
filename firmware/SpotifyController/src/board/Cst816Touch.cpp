#include "Cst816Touch.h"

#include "TouchTransform.h"

namespace spotctl {

namespace {
constexpr uint8_t kAddress = 0x15;
constexpr uint8_t kChipIdRegister = 0xA7;
constexpr uint8_t kTouchCountRegister = 0x02;
constexpr uint8_t kPositionRegister = 0x03;
} // namespace

bool Cst816Touch::begin(TwoWire &wire, uint8_t rotation, uint16_t width,
                        uint16_t height) {
  wire_ = &wire;
  rotation_ = rotation;
  width_ = width;
  height_ = height;

  uint8_t chip_id = 0;
  detected_ = readRegisters(kChipIdRegister, &chip_id, 1) && chip_id == 0xB6;
  return detected_;
}

bool Cst816Touch::readRegisters(uint8_t reg, uint8_t *data, size_t length) {
  if (wire_ == nullptr) {
    return false;
  }
  wire_->beginTransmission(kAddress);
  wire_->write(reg);
  if (wire_->endTransmission(true) != 0) {
    return false;
  }
  const size_t received = wire_->requestFrom(kAddress, length);
  if (received != length) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    data[index] = static_cast<uint8_t>(wire_->read());
  }
  return true;
}

bool Cst816Touch::read(uint16_t &x, uint16_t &y) {
  uint8_t touch_count = 0;
  if (!readRegisters(kTouchCountRegister, &touch_count, 1) || touch_count == 0) {
    return false;
  }

  uint8_t coordinates[4]{};
  if (!readRegisters(kPositionRegister, coordinates, sizeof(coordinates))) {
    return false;
  }
  const uint16_t raw_x =
      static_cast<uint16_t>(((coordinates[0] & 0x0F) << 8) | coordinates[1]);
  const uint16_t raw_y =
      static_cast<uint16_t>(((coordinates[2] & 0x0F) << 8) | coordinates[3]);
  transform(raw_x, raw_y, x, y);
  return x < width_ && y < height_;
}

void Cst816Touch::transform(uint16_t raw_x, uint16_t raw_y, uint16_t &x,
                            uint16_t &y) const {
  transformTouchPoint(raw_x, raw_y, rotation_, width_, height_, x, y);
}

} // namespace spotctl
