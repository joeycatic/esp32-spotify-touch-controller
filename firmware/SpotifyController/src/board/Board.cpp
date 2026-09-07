#include "Board.h"

#include <Wire.h>
#include <esp_heap_caps.h>

#include "BoardDetector.h"
#include "Compact2Backend.h"
#include "Wide7BBackend.h"

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

  BoardDetector detector(Wire);
  status_.profile = detector.detect();
  const DetectionEvidence &evidence = detector.evidence();
  char probe_line[160];
  snprintf(probe_line, sizeof(probe_line),
           "[board] probes 7B(GPIO8/9 expander=%s GT911=%s) compact(GPIO48/47 CST816=%s)",
           evidence.wide_expander ? "ok" : "no",
           evidence.wide_gt911 ? "ok" : "no",
           evidence.compact_cst816 ? "ok" : "no");
  logBoth(probe_line);
  capabilities_ = capabilitiesFor(status_.profile);
  logDetection();
  if (status_.profile == BoardProfile::Compact2) {
    backend_ = new Compact2Backend();
  } else if (status_.profile == BoardProfile::Wide7B) {
    backend_ = new Wide7BBackend();
  } else {
    logBoth("[board] No unambiguous supported display was detected");
    return false;
  }
  if (backend_ == nullptr || !backend_->begin()) {
    logBoth("[board] Display backend initialization failed");
    return false;
  }
  status_.touch_ready = backend_->touchReady();

  lv_init();
  const size_t pixel_count = static_cast<size_t>(width()) *
                             capabilities_.display.draw_buffer_rows;
  buffer_a_ = static_cast<lv_color_t *>(
      heap_caps_malloc(pixel_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  buffer_b_ = static_cast<lv_color_t *>(
      heap_caps_malloc(pixel_count * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  if ((buffer_a_ == nullptr || buffer_b_ == nullptr) && !wide()) {
    if (buffer_a_ != nullptr) {
      heap_caps_free(buffer_a_);
    }
    if (buffer_b_ != nullptr) {
      heap_caps_free(buffer_b_);
    }
    buffer_a_ = static_cast<lv_color_t *>(heap_caps_malloc(
        pixel_count * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    buffer_b_ = static_cast<lv_color_t *>(heap_caps_malloc(
        pixel_count * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (buffer_a_ == nullptr || buffer_b_ == nullptr) {
    logBoth("[board] LVGL draw-buffer allocation failed");
    return false;
  }

  lv_disp_draw_buf_init(&draw_buffer_, buffer_a_, buffer_b_, pixel_count);
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
  lv_obj_set_style_text_font(boot_title_,
                             wide() ? &lv_font_montserrat_24
                                    : &lv_font_montserrat_20,
                             0);
  lv_obj_align(boot_title_, LV_ALIGN_CENTER, 0, -18);
  boot_detail_ = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_color(boot_detail_, lv_color_hex(0xB3B3B3), 0);
  lv_obj_set_style_text_align(boot_detail_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(boot_detail_, wide() ? 700 : 216);
  lv_obj_align(boot_detail_, LV_ALIGN_CENTER, 0, 18);
  showBootMessage("Starting", "Checking configuration...");
  status_.display_ready = true;
  lv_timer_handler();
  setBacklight(wide() ? 100 : 70);
  return true;
}

void Board::logDetection() const {
  char line[160];
  snprintf(line, sizeof(line),
           "[board] profile=%s flash=%uMB psram=%uMB boot=%s",
           boardProfileName(status_.profile),
           static_cast<unsigned>(status_.flash_bytes / (1024U * 1024U)),
           static_cast<unsigned>(status_.psram_bytes / (1024U * 1024U)),
           boot_held_at_start_ ? "held" : "released");
  logBoth(line);
}

Stream &Board::primarySerial() const {
  return (capabilities_.serial == SerialTransport::Uart0 ||
          status_.profile == BoardProfile::Unknown)
             ? static_cast<Stream &>(Serial0)
             : static_cast<Stream &>(Serial);
}

void Board::tick() {
  if (status_.display_ready) {
    lv_timer_handler();
  }
}

void Board::setBacklight(uint8_t percent) {
  if (backend_ != nullptr) {
    backend_->setBacklight(percent);
  }
}

bool Board::bootButtonHeld() const {
  return wide() ? boot_held_at_start_ : digitalRead(kPinBoot) == LOW;
}

void Board::showBootMessage(const char *title, const char *detail) {
  if (boot_title_ == nullptr || boot_detail_ == nullptr) {
    return;
  }
  lv_label_set_text(boot_title_, title);
  lv_label_set_text(boot_detail_, detail);
}

void Board::flushCallback(lv_disp_drv_t *, const lv_area_t *area,
                          lv_color_t *colors) {
  if (instance_ != nullptr && instance_->backend_ != nullptr) {
    instance_->backend_->draw(*area, colors);
  }
  if (instance_ != nullptr) {
    lv_disp_flush_ready(&instance_->display_driver_);
  }
}

void Board::touchCallback(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint16_t x = 0;
  uint16_t y = 0;
  if (instance_ != nullptr && instance_->backend_ != nullptr &&
      instance_->backend_->readTouch(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = static_cast<lv_coord_t>(x);
    data->point.y = static_cast<lv_coord_t>(y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

} // namespace spotctl
