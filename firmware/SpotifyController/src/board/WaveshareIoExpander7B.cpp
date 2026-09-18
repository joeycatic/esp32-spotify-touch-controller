#include "WaveshareIoExpander7B.h"

namespace spotctl {

namespace {
constexpr uint8_t kAddress = 0x24;
constexpr uint8_t kModeRegister = 0x02;
constexpr uint8_t kOutputRegister = 0x03;
constexpr uint8_t kPwmRegister = 0x05;
} // namespace

bool WaveshareIoExpander7B::writeRegister(uint8_t reg, uint8_t value) {
  if (wire_ == nullptr) {
    return false;
  }
  wire_->beginTransmission(kAddress);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission(true) == 0;
}

bool WaveshareIoExpander7B::begin(TwoWire &wire) {
  wire_ = &wire;
  if (!writeRegister(kModeRegister, 0xFF)) {
    return false;
  }
  // Preserve the panel's power-on state. The backend releases touch reset after
  // selecting its I2C address; LCD reset and VCOM remain high throughout.
  output_ = startupOutputs(false);
  // IO2 keeps the backlight physically disabled. Leave the PWM register
  // untouched here, matching Waveshare's LCD bring-up sequence; it is set
  // immediately before IO2 is enabled by setBacklight().
  return writeRegister(kOutputRegister, output_);
}

// The expander keeps its own power and latches its outputs, so a warm reset --
// which is how every flash and every serial-triggered reboot ends -- leaves the
// panel running with whatever state it held when the CPU was reset. A startup
// mask that already has LCD reset high never disturbs it, and the panel scans
// out nothing. Waveshare's FAQ answers that black screen with "disconnect and
// reconnect power"; one reset edge achieves the same thing without dropping the
// panel rail, which fights the panel's own power-on sequence.
bool WaveshareIoExpander7B::pulseLcdReset() {
  if (!set(LcdReset, false)) {
    return false;
  }
  delay(20);
  if (!set(LcdReset, true)) {
    return false;
  }
  // The panel runs its own initialization from this edge before it will accept
  // pixel data; the RGB peripheral starts well after this returns.
  delay(120);
  return true;
}

bool WaveshareIoExpander7B::set(Pin pin, bool high) {
  if (high) {
    output_ |= static_cast<uint8_t>(1U << pin);
  } else {
    output_ &= static_cast<uint8_t>(~(1U << pin));
  }
  return writeRegister(kOutputRegister, output_);
}

bool WaveshareIoExpander7B::setBacklight(uint8_t percent) {
  const uint8_t bounded = percent > 100 ? 100 : percent;
  if (bounded == 0) {
    return writeRegister(kPwmRegister, 255) && set(BacklightEnable, false);
  }
  const uint8_t inverted =
      static_cast<uint8_t>((100U - bounded) * 255U / 100U);
  return writeRegister(kPwmRegister, inverted) && set(BacklightEnable, true);
}

} // namespace spotctl
