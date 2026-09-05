#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "Cst816Touch.h"

namespace spotctl {

struct HardwareStatus {
  bool display_ready{false};
  bool touch_ready{false};
  size_t flash_bytes{0};
  size_t psram_bytes{0};
};

class Board {
public:
  Board();

  bool begin();
  void tick();
  void setBacklight(uint8_t percent);
  bool bootButtonHeld() const;
  void showBootMessage(const char *title, const char *detail);

  const HardwareStatus &status() const { return status_; }
  lv_indev_t *inputDevice() const { return input_device_; }

private:
  static void flushCallback(lv_disp_drv_t *driver, const lv_area_t *area,
                            lv_color_t *colors);
  static void touchCallback(lv_indev_drv_t *driver, lv_indev_data_t *data);

  static Board *instance_;
  Arduino_ESP32SPI *bus_{nullptr};
  Arduino_ST7789 *display_{nullptr};
  Cst816Touch touch_;
  lv_disp_draw_buf_t draw_buffer_{};
  lv_disp_drv_t display_driver_{};
  lv_indev_drv_t input_driver_{};
  lv_indev_t *input_device_{nullptr};
  lv_color_t *buffer_a_{nullptr};
  lv_color_t *buffer_b_{nullptr};
  lv_obj_t *boot_title_{nullptr};
  lv_obj_t *boot_detail_{nullptr};
  HardwareStatus status_;
};

} // namespace spotctl
