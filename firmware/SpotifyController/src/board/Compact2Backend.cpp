#include "Compact2Backend.h"

#include <Wire.h>

namespace spotctl {

bool Compact2Backend::begin() {
  bus_ = new Arduino_ESP32SPI(42, 45, 39, 38, 40);
  display_ = new Arduino_ST7789(bus_, -1, 0, true, 240, 320);
  if (bus_ == nullptr || display_ == nullptr || !display_->begin()) {
    return false;
  }
  display_->fillScreen(RGB565_BLACK);
  ledcAttach(1, 5000, 10);
  Wire.end();
  Wire.begin(48, 47, 400000);
  touch_ready_ = touch_.begin(Wire, 0, 240, 320);
  return true;
}

void Compact2Backend::draw(const lv_area_t &area, const lv_color_t *colors) {
  if (display_ == nullptr) {
    return;
  }
  display_->draw16bitRGBBitmap(
      area.x1, area.y1, reinterpret_cast<const uint16_t *>(colors),
      area.x2 - area.x1 + 1, area.y2 - area.y1 + 1);
}

bool Compact2Backend::readTouch(uint16_t &x, uint16_t &y) {
  return touch_.read(x, y);
}

void Compact2Backend::setBacklight(uint8_t percent) {
  const uint8_t bounded = percent > 100 ? 100 : percent;
  ledcWrite(1, static_cast<uint32_t>(bounded) * 1023U / 100U);
}

} // namespace spotctl
