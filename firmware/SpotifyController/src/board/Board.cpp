#include "Board.h"

#include <esp_heap_caps.h>

namespace spotctl {

namespace {
constexpr int kPinBoot = 0;

void logBoth(const char *message) {
  Serial.println(message);
  Serial0.println(message);
}
} // namespace

Board *Board::instance_ = nullptr;

Board::Board() { instance_ = this; }

bool Board::begin() {
  pinMode(kPinBoot, INPUT_PULLUP);
  boot_held_at_start_ = digitalRead(kPinBoot) == LOW;
  status_.flash_bytes = ESP.getFlashChipSize();
  status_.psram_bytes = ESP.getPsramSize();
  logHardware();

  if (!backend_.begin()) {
    logBoth("[board] Display backend initialization failed");
    return false;
  }
  status_.touch_ready = backend_.touchReady();

  lv_init();
  const size_t pixel_count = static_cast<size_t>(width()) *
                             kBoardCapabilities.display.draw_buffer_rows;
  buffer_a_ = allocateDrawBuffer(pixel_count * sizeof(lv_color_t));
  if (buffer_a_ == nullptr) {
    logBoth("[board] LVGL draw-buffer allocation failed");
    return false;
  }
  char buffer_line[160];
  snprintf(buffer_line, sizeof(buffer_line),
           "[board] LVGL draw buffer: %u x %u rows, 1 buffer, %s",
           static_cast<unsigned>(width()),
           static_cast<unsigned>(kBoardCapabilities.display.draw_buffer_rows),
           draw_buffers_internal_ ? "internal RAM" : "PSRAM (degraded)");
  logBoth(buffer_line);
  lv_disp_draw_buf_init(&draw_buffer_, buffer_a_, nullptr, pixel_count);

  lv_disp_drv_init(&display_driver_);
  display_driver_.hor_res = width();
  display_driver_.ver_res = height();
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
  lv_obj_set_style_text_font(boot_title_, &lv_font_montserrat_24, 0);
  lv_obj_align(boot_title_, LV_ALIGN_CENTER, 0, -18);
  boot_detail_ = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(boot_detail_, lv_color_hex(0xB3B3B3), 0);
  lv_obj_set_style_text_align(boot_detail_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(boot_detail_, 700);
  lv_obj_align(boot_detail_, LV_ALIGN_CENTER, 0, 18);
  showBootMessage("Starting", "Checking configuration...");
  status_.display_ready = true;
  lv_timer_handler();
  setBacklight(100);
  return true;
}

// Keep LVGL's render target out of PSRAM. The panel's bounce-buffer ISR must
// copy the framebuffer out of PSRAM against a fixed per-scanline deadline while
// the refresh itself already consumes about 40 MB/s of the bus. Rendering into
// PSRAM makes the flush a PSRAM-to-PSRAM copy, which costs two bytes of bus
// traffic per byte moved; measured on hardware that halves flush throughput
// from 15 MB/s to 7.5 MB/s and starves the refill ISR into tearing.
// Internal RAM is therefore tried first. PSRAM stays as a fallback so a tight
// internal heap degrades rather than leaving the board without a display, and
// the boot log says which one was used.
lv_color_t *Board::allocateDrawBuffer(size_t bytes) {
  void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buffer == nullptr) {
    buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    draw_buffers_internal_ = false;
  }
  return static_cast<lv_color_t *>(buffer);
}

void Board::logHardware() const {
  char line[160];
  snprintf(line, sizeof(line), "[board] %s flash=%uMB psram=%uMB boot=%s",
           kBoardCapabilities.model,
           static_cast<unsigned>(status_.flash_bytes / (1024U * 1024U)),
           static_cast<unsigned>(status_.psram_bytes / (1024U * 1024U)),
           boot_held_at_start_ ? "held" : "released");
  logBoth(line);
}

Stream &Board::primarySerial() const { return Serial0; }

void Board::tick() {
  if (status_.display_ready) {
    lv_timer_handler();
  }
  backend_.poll();
}

void Board::setBacklight(uint8_t percent) { backend_.setBacklight(percent); }

bool Board::bootButtonHeld() const { return boot_held_at_start_; }

void Board::showBootMessage(const char *title, const char *detail) {
  if (boot_title_ == nullptr || boot_detail_ == nullptr) {
    return;
  }
  lv_label_set_text(boot_title_, title);
  lv_label_set_text(boot_detail_, detail);
}

void Board::flushCallback(lv_disp_drv_t *, const lv_area_t *area,
                          lv_color_t *colors) {
  if (instance_ != nullptr) {
    instance_->backend_.draw(*area, colors);
    lv_disp_flush_ready(&instance_->display_driver_);
  }
}

void Board::touchCallback(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t x = 0;
  uint16_t y = 0;
  if (instance_ != nullptr && instance_->backend_.readTouch(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = static_cast<lv_coord_t>(x);
    data->point.y = static_cast<lv_coord_t>(y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

} // namespace spotctl
