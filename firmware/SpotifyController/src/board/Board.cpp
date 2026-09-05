#include "Board.h"

#include <esp_heap_caps.h>

namespace spotctl {

namespace {
constexpr int kWidth = 240;
constexpr int kHeight = 320;
constexpr int kBufferRows = 40;
constexpr int kPinSclk = 39;
constexpr int kPinMosi = 38;
constexpr int kPinMiso = 40;
constexpr int kPinDc = 42;
constexpr int kPinCs = 45;
constexpr int kPinReset = -1;
constexpr int kPinBacklight = 1;
constexpr int kPinTouchSda = 48;
constexpr int kPinTouchScl = 47;
constexpr int kPinBoot = 0;
} // namespace

Board *Board::instance_ = nullptr;

Board::Board() { instance_ = this; }

bool Board::begin() {
  pinMode(kPinBoot, INPUT_PULLUP);
  bus_ = new Arduino_ESP32SPI(kPinDc, kPinCs, kPinSclk, kPinMosi, kPinMiso);
  display_ = new Arduino_ST7789(bus_, kPinReset, 0, true, kWidth, kHeight);
  status_.display_ready = display_ != nullptr && display_->begin();
  if (!status_.display_ready) {
    return false;
  }
  display_->fillScreen(RGB565_BLACK);

  ledcAttach(kPinBacklight, 5000, 10);
  setBacklight(70);

  Wire.begin(kPinTouchSda, kPinTouchScl);
  status_.touch_ready = touch_.begin(Wire, 0, kWidth, kHeight);
  status_.flash_bytes = ESP.getFlashChipSize();
  status_.psram_bytes = ESP.getPsramSize();

  lv_init();
  const size_t pixel_count = static_cast<size_t>(kWidth * kBufferRows);
  buffer_a_ = static_cast<lv_color_t *>(
      heap_caps_malloc(pixel_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  buffer_b_ = static_cast<lv_color_t *>(
      heap_caps_malloc(pixel_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  if (buffer_a_ == nullptr || buffer_b_ == nullptr) {
    return false;
  }

  lv_disp_draw_buf_init(&draw_buffer_, buffer_a_, buffer_b_, pixel_count);
  lv_disp_drv_init(&display_driver_);
  display_driver_.hor_res = kWidth;
  display_driver_.ver_res = kHeight;
  display_driver_.flush_cb = flushCallback;
  display_driver_.draw_buf = &draw_buffer_;
  lv_disp_drv_register(&display_driver_);

  lv_indev_drv_init(&input_driver_);
  input_driver_.type = LV_INDEV_TYPE_POINTER;
  input_driver_.read_cb = touchCallback;
  input_device_ = lv_indev_drv_register(&input_driver_);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x080A0C), 0);
  boot_title_ = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(boot_title_, lv_color_hex(0x1ED760), 0);
  lv_obj_set_style_text_font(boot_title_, &lv_font_montserrat_20, 0);
  lv_obj_align(boot_title_, LV_ALIGN_CENTER, 0, -18);
  boot_detail_ = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(boot_detail_, lv_color_hex(0xB3B3B3), 0);
  lv_obj_set_style_text_align(boot_detail_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(boot_detail_, 216);
  lv_obj_align(boot_detail_, LV_ALIGN_CENTER, 0, 18);
  showBootMessage("Starting", "Checking hardware...");
  return true;
}

void Board::tick() { lv_timer_handler(); }

void Board::setBacklight(uint8_t percent) {
  const uint8_t bounded = percent > 100 ? 100 : percent;
  ledcWrite(kPinBacklight, static_cast<uint32_t>(bounded) * 1023U / 100U);
}

bool Board::bootButtonHeld() const { return digitalRead(kPinBoot) == LOW; }

void Board::showBootMessage(const char *title, const char *detail) {
  if (boot_title_ == nullptr || boot_detail_ == nullptr) {
    return;
  }
  lv_label_set_text(boot_title_, title);
  lv_label_set_text(boot_detail_, detail);
}

void Board::flushCallback(lv_disp_drv_t *, const lv_area_t *area,
                          lv_color_t *colors) {
  if (instance_ == nullptr || instance_->display_ == nullptr) {
    return;
  }
  const int width = area->x2 - area->x1 + 1;
  const int height = area->y2 - area->y1 + 1;
  instance_->display_->draw16bitRGBBitmap(
      area->x1, area->y1, reinterpret_cast<uint16_t *>(colors), width, height);
  lv_disp_flush_ready(&instance_->display_driver_);
}

void Board::touchCallback(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t x = 0;
  uint16_t y = 0;
  if (instance_ != nullptr && instance_->touch_.read(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = static_cast<lv_coord_t>(x);
    data->point.y = static_cast<lv_coord_t>(y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

} // namespace spotctl
