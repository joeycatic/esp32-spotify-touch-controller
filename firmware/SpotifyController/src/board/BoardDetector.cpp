#include "BoardDetector.h"

#include <Preferences.h>

namespace spotctl {

namespace {
constexpr char kNamespace[] = "spotctl_hw";
constexpr uint8_t kSchema = 1;
constexpr uint8_t kWideAddress = 0x24;
constexpr uint8_t kGt911Address = 0x5D;
constexpr uint8_t kCst816Address = 0x15;
constexpr uint8_t kWideModeRegister = 0x02;
constexpr uint8_t kWideOutputRegister = 0x03;
constexpr uint8_t kWidePwmRegister = 0x05;
constexpr uint8_t kWideSafeOutputs = 0xD1;
constexpr uint8_t kWideTouchReadyOutputs = 0xD3;
} // namespace

BoardProfile BoardProfileStore::load() const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return BoardProfile::Unknown;
  }
  const bool valid = preferences.getUChar("schema", 0) == kSchema;
  const uint8_t stored = preferences.getUChar("profile", 0);
  preferences.end();
  if (!valid || stored > static_cast<uint8_t>(BoardProfile::Wide7B)) {
    return BoardProfile::Unknown;
  }
  return static_cast<BoardProfile>(stored);
}

bool BoardProfileStore::save(BoardProfile profile) const {
  if (profile == BoardProfile::Unknown) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool saved = preferences.putUChar("profile", static_cast<uint8_t>(profile)) == 1 &&
                     preferences.putUChar("schema", kSchema) == 1;
  preferences.end();
  return saved;
}

bool BoardDetector::write8(uint8_t address, uint8_t reg, uint8_t value) {
  wire_.beginTransmission(address);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission(true) == 0;
}

bool BoardDetector::read8(uint8_t address, uint8_t reg, uint8_t *data,
                          size_t length) {
  wire_.beginTransmission(address);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0 ||
      wire_.requestFrom(address, length) != length) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    data[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

bool BoardDetector::read16(uint8_t address, uint16_t reg, uint8_t *data,
                           size_t length) {
  wire_.beginTransmission(address);
  wire_.write(static_cast<uint8_t>(reg >> 8));
  wire_.write(static_cast<uint8_t>(reg & 0xFF));
  if (wire_.endTransmission(false) != 0 ||
      wire_.requestFrom(address, length) != length) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    data[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

bool BoardDetector::probeWide() {
  wire_.end();
  wire_.begin(8, 9, 400000);
  uint8_t adc[2]{};
  uint8_t product[3]{};
  evidence_.wide_expander =
      read8(kWideAddress, 0x06, adc, sizeof(adc)) &&
      write8(kWideAddress, kWideModeRegister, 0xFF) &&
      write8(kWideAddress, kWideOutputRegister, kWideSafeOutputs) &&
      write8(kWideAddress, kWidePwmRegister, 0xFF);
  if (!evidence_.wide_expander) {
    return false;
  }

  // EXIO1 holds the 7B's GT911 in reset. Select the documented 0x5D
  // address with GPIO4 low, release reset, and only then verify product ID.
  // LCD reset and the backlight stay disabled until the backend is selected.
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  delay(100);
  if (!write8(kWideAddress, kWideOutputRegister,
              kWideTouchReadyOutputs)) {
    evidence_.wide_expander = false;
    pinMode(4, INPUT_PULLUP);
    return false;
  }
  delay(200);
  pinMode(4, INPUT_PULLUP);
  evidence_.wide_gt911 =
      read16(kGt911Address, 0x8140, product, sizeof(product)) &&
      product[0] == '9' && product[1] == '1' && product[2] == '1';
  return evidence_.wide_expander && evidence_.wide_gt911;
}

bool BoardDetector::probeCompact() {
  wire_.end();
  wire_.begin(48, 47, 400000);
  uint8_t chip_id = 0;
  evidence_.compact_cst816 =
      read8(kCst816Address, 0xA7, &chip_id, 1) && chip_id == 0xB6;
  return evidence_.compact_cst816;
}

bool BoardDetector::verify(BoardProfile profile) {
  evidence_ = {};
  if (profile == BoardProfile::Wide7B) {
    return probeWide();
  }
  if (profile == BoardProfile::Compact2) {
    return probeCompact();
  }
  return false;
}

BoardProfile BoardDetector::detect() {
  const BoardProfile remembered = store_.load();
  if (remembered != BoardProfile::Unknown && verify(remembered)) {
    return remembered;
  }

  evidence_ = {};
  probeWide();
  probeCompact();
  const BoardProfile detected = selectDetectedProfile(evidence_);
  if (detected != BoardProfile::Unknown) {
    store_.save(detected);
  }
  return detected;
}

} // namespace spotctl
