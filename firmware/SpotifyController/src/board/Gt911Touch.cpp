#include "Gt911Touch.h"

namespace spotctl {

namespace {
constexpr uint8_t kAddress = 0x5D;
constexpr uint16_t kProductIdRegister = 0x8140;
constexpr uint16_t kTouchStatusRegister = 0x814E;
} // namespace

bool Gt911Touch::readRegisters(uint16_t reg, uint8_t *data, size_t length) {
  if (wire_ == nullptr) {
    return false;
  }
  wire_->beginTransmission(kAddress);
  wire_->write(static_cast<uint8_t>(reg >> 8));
  wire_->write(static_cast<uint8_t>(reg & 0xFF));
  if (wire_->endTransmission(false) != 0 ||
      wire_->requestFrom(kAddress, length) != length) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    data[i] = static_cast<uint8_t>(wire_->read());
  }
  return true;
}

bool Gt911Touch::writeRegister(uint16_t reg, uint8_t value) {
  if (wire_ == nullptr) {
    return false;
  }
  wire_->beginTransmission(kAddress);
  wire_->write(static_cast<uint8_t>(reg >> 8));
  wire_->write(static_cast<uint8_t>(reg & 0xFF));
  wire_->write(value);
  return wire_->endTransmission(true) == 0;
}

bool Gt911Touch::begin(TwoWire &wire, uint16_t width, uint16_t height) {
  wire_ = &wire;
  width_ = width;
  height_ = height;
  uint8_t product[3]{};
  return readRegisters(kProductIdRegister, product, sizeof(product)) &&
         product[0] == '9' && product[1] == '1' && product[2] == '1';
}

bool Gt911Touch::read(uint16_t &x, uint16_t &y) {
  uint8_t status = 0;
  if (!readRegisters(kTouchStatusRegister, &status, 1)) {
    return false;
  }
  const uint8_t count = status & 0x0F;
  if ((status & 0x80) == 0 || count == 0 || count > 5) {
    if (status != 0) {
      writeRegister(kTouchStatusRegister, 0);
    }
    return false;
  }
  uint8_t point[8]{};
  const bool read_ok =
      readRegisters(kTouchStatusRegister + 1, point, sizeof(point));
  writeRegister(kTouchStatusRegister, 0);
  if (!read_ok) {
    return false;
  }
  x = static_cast<uint16_t>(point[1] | (point[2] << 8));
  y = static_cast<uint16_t>(point[3] | (point[4] << 8));
  return x < width_ && y < height_;
}

} // namespace spotctl
