#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "BoardProfile.h"
#include "Wide7BBackend.h"

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

  const BoardCapabilities &capabilities() const { return kBoardCapabilities; }
  uint16_t width() const { return kBoardCapabilities.display.width; }
  uint16_t height() const { return kBoardCapabilities.display.height; }
  // The 7B's own USB port is shared with CAN and may not enumerate before
  // firmware starts, so the console is always UART1 on Serial0.
  Stream &primarySerial() const;
  const HardwareStatus &status() const { return status_; }
  lv_indev_t *inputDevice() const { return input_device_; }

private:
  static void flushCallback(lv_disp_drv_t *driver, const lv_area_t *area,
                            lv_color_t *colors);
  static void touchCallback(lv_indev_drv_t *driver, lv_indev_data_t *data);
  lv_color_t *allocateDrawBuffer(size_t bytes);
  void logHardware() const;

  static Board *instance_;
  Wide7BBackend backend_;
  lv_disp_draw_buf_t draw_buffer_{};
  lv_disp_drv_t display_driver_{};
  lv_indev_drv_t input_driver_{};
  lv_indev_t *input_device_{nullptr};
  lv_color_t *buffer_a_{nullptr};
  lv_obj_t *boot_title_{nullptr};
  lv_obj_t *boot_detail_{nullptr};
  HardwareStatus status_;
  bool boot_held_at_start_{false};
  bool draw_buffers_internal_{true};
};

} // namespace spotctl
