#include "Wide7BBackend.h"

#include <Wire.h>
#include <cstring>

namespace spotctl {

namespace {
void log7B(const char *message) {
  Serial.print("[7B] ");
  Serial.println(message);
  Serial0.print("[7B] ");
  Serial0.println(message);
}

void log7BError(const char *stage, esp_err_t error) {
  char message[96];
  snprintf(message, sizeof(message), "%s failed: %s (0x%x)", stage,
           esp_err_to_name(error), static_cast<unsigned>(error));
  log7B(message);
}
} // namespace

bool Wide7BBackend::begin() {
#ifdef SPOTCTL_VENDOR_MEMORY_PROFILE
  log7B("source-built SDK: 120 MHz PSRAM (temp-tracked), 64 KB data cache, "
        "64-byte lines, 32 KB instruction cache, PSRAM code/rodata");
#endif
  Wire.end();
  Wire.begin(8, 9, 400000);
  if (!expander_.begin(Wire)) {
    log7B("I/O expander initialization failed");
    return false;
  }
  log7B("I/O expander ready; display power on, backlight off");

  pinMode(4, OUTPUT);
  expander_.set(WaveshareIoExpander7B::TouchReset, false);
  expander_.set(WaveshareIoExpander7B::LcdReset, false);
  // The expander keeps its own power and latches its outputs, so a warm reset
  // (which is how every flash and every serial-triggered reboot ends) leaves the
  // panel rail already enabled and the panel running with whatever state it had
  // when the CPU was reset. Waveshare's FAQ answers the resulting black screen
  // with "disconnect and reconnect power"; dropping the rail here does that in
  // firmware, so a warm reset reaches the panel as a cold start.
  expander_.set(WaveshareIoExpander7B::LcdPowerEnable, false);
  delay(60);
  expander_.set(WaveshareIoExpander7B::LcdPowerEnable, true);
  delay(60);
  log7B("panel rail power-cycled");
  expander_.set(WaveshareIoExpander7B::LcdReset, true);
  delay(100);
  digitalWrite(4, LOW);
  delay(100);
  expander_.set(WaveshareIoExpander7B::TouchReset, true);
  delay(200);
  pinMode(4, INPUT_PULLUP);
  touch_ready_ = touch_.begin(Wire, 1024, 600);
  log7B(touch_ready_ ? "GT911 ready" : "GT911 unavailable");

  esp_lcd_rgb_panel_config_t config{};
  config.clk_src = LCD_CLK_SRC_DEFAULT;
  config.timings.pclk_hz = 30U * 1000U * 1000U;
  config.timings.h_res = 1024;
  config.timings.v_res = 600;
  config.timings.hsync_pulse_width = 162;
  config.timings.hsync_back_porch = 152;
  config.timings.hsync_front_porch = 48;
  config.timings.vsync_pulse_width = 45;
  config.timings.vsync_back_porch = 13;
  config.timings.vsync_front_porch = 3;
  config.timings.flags.pclk_active_neg = 1;
  config.data_width = 16;
  config.bits_per_pixel = 16;
  config.num_fbs = 1;
  // Bounce buffers are mandatory here: with the framebuffer in PSRAM, direct
  // GDMA scanout cannot sustain this panel and the display stays dark.
  // Ten scanlines is Waveshare's value and the only size this board has ever
  // been seen working with; the twenty- and thirty-line trials each cost 40 and
  // 80 KiB more internal RAM and each ended in a black screen. The refill
  // deadline is not the binding constraint anyway -- PSRAM read bandwidth is --
  // so buy headroom with the cache and draw-buffer settings, not with a bigger
  // bounce buffer. 1024x600 must stay an exact multiple of this, as the driver
  // requires.
  config.bounce_buffer_size_px = 1024U * 10U;
  config.dma_burst_size = 64;
  config.hsync_gpio_num = 46;
  config.vsync_gpio_num = 3;
  config.de_gpio_num = 5;
  config.pclk_gpio_num = 7;
  config.disp_gpio_num = -1;
  const int pins[16] = {14, 38, 18, 17, 10, 39, 0, 45,
                        48, 47, 21, 1,  2,  42, 41, 40};
  for (size_t i = 0; i < 16; ++i) {
    config.data_gpio_nums[i] = pins[i];
  }
  config.flags.fb_in_psram = 1;
  log7B("RGB timing: 30 MHz pixel clock, 10-line bounce buffers");
  log7B("allocating RGB panel framebuffer");
  const esp_err_t create_error = esp_lcd_new_rgb_panel(&config, &panel_);
  if (create_error != ESP_OK || panel_ == nullptr) {
    log7BError("esp_lcd_new_rgb_panel", create_error);
    return false;
  }
  log7B("RGB panel allocated");
  const esp_err_t init_error = esp_lcd_panel_init(panel_);
  if (init_error != ESP_OK) {
    log7BError("esp_lcd_panel_init", init_error);
    esp_lcd_panel_del(panel_);
    panel_ = nullptr;
    return false;
  }
  log7B("RGB panel initialized");
  const esp_err_t framebuffer_error =
      esp_lcd_rgb_panel_get_frame_buffer(panel_, 1, &frame_buffer_);
  if (framebuffer_error != ESP_OK || frame_buffer_ == nullptr) {
    log7BError("esp_lcd_rgb_panel_get_frame_buffer", framebuffer_error);
    esp_lcd_panel_del(panel_);
    panel_ = nullptr;
    return false;
  }
  std::memset(frame_buffer_, 0, 1024U * 600U * sizeof(uint16_t));
  log7B("framebuffer cleared; panel ready");
  return true;
}

// This board runs one framebuffer. A second one would let the driver latch the
// swap at the frame boundary (lcd_rgb_panel_fill_bounce_buffer only moves
// bb_fb_index to cur_fb_index when bounce_pos_px wraps), which is genuinely
// tear-free -- but reaching it means LVGL must render full frames straight into
// PSRAM, adding roughly 1.2 MB of PSRAM writes per frame to a bus whose
// saturation is the actual defect. Until the memory profile has headroom to
// spare, one framebuffer and a small internal draw buffer move the least data.
void Wide7BBackend::draw(const lv_area_t &area, const lv_color_t *colors) {
  if (panel_ == nullptr || frame_buffer_ == nullptr) {
    return;
  }
  // In bounce-buffer mode the driver copies the LVGL buffer into the PSRAM
  // framebuffer line by line and deliberately skips the cache sync: the refill
  // ISR reads the framebuffer through the same cache, so no flush is needed.
  const uint32_t started_us = micros();
  const esp_err_t error = esp_lcd_panel_draw_bitmap(
      panel_, area.x1, area.y1, area.x2 + 1, area.y2 + 1, colors);
  flush_us_ += micros() - started_us;
  flush_bytes_ += static_cast<uint32_t>(area.x2 - area.x1 + 1) *
                  static_cast<uint32_t>(area.y2 - area.y1 + 1) *
                  sizeof(uint16_t);
  ++flush_count_;
  if (!first_draw_logged_) {
    if (error == ESP_OK) {
      log7B("first LVGL framebuffer flush complete");
    } else {
      log7BError("first LVGL framebuffer flush", error);
    }
    first_draw_logged_ = true;
  }
}

void Wide7BBackend::poll() { reportFlushThroughput(); }

// Flush throughput is the one directly observable symptom of the PSRAM
// contention that starves the bounce-buffer refill ISR and produces torn
// scanlines. A healthy copy into PSRAM sustains tens of MB/s; a number that
// collapses while artwork loads means the bus, not the panel timing, is the
// limit.
void Wide7BBackend::reportFlushThroughput() {
  constexpr uint32_t kReportIntervalMs = 5000;
  const uint32_t now = millis();
  if (report_started_ms_ == 0) {
    report_started_ms_ = now;
    return;
  }
  if (now - report_started_ms_ < kReportIntervalMs || flush_us_ == 0) {
    return;
  }
  char message[128];
  snprintf(message, sizeof(message),
           "flush %ux %uKB in %ums -> %u KB/s copied into PSRAM",
           static_cast<unsigned>(flush_count_),
           static_cast<unsigned>(flush_bytes_ / 1024U),
           static_cast<unsigned>(flush_us_ / 1000U),
           static_cast<unsigned>(static_cast<uint64_t>(flush_bytes_) *
                                 1000000U / flush_us_ / 1024U));
  log7B(message);
  report_started_ms_ = now;
  flush_us_ = 0;
  flush_bytes_ = 0;
  flush_count_ = 0;
}

bool Wide7BBackend::readTouch(uint16_t &x, uint16_t &y) {
  return touch_ready_ && touch_.read(x, y);
}

void Wide7BBackend::setBacklight(uint8_t percent) {
  if (expander_.setBacklight(percent)) {
    log7B(percent == 0 ? "backlight off" : "backlight on");
  } else {
    log7B("backlight update failed");
  }
}

} // namespace spotctl
