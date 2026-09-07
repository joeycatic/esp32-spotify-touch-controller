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
  delay(20);
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
  // Waveshare's ten-scanline buffers are required to keep the 30 MHz RGB DMA
  // fed consistently from PSRAM. TLS headroom is reserved in LVGL's heap
  // budget instead of shrinking these timing-sensitive buffers.
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

void Wide7BBackend::draw(const lv_area_t &area, const lv_color_t *colors) {
  if (panel_ == nullptr || frame_buffer_ == nullptr) {
    return;
  }
  // Let the RGB driver copy partial LVGL buffers into its PSRAM framebuffer.
  // Besides clipping and stride handling, this performs the cache sync needed
  // before LCD DMA reads the updated pixels.
  const esp_err_t error = esp_lcd_panel_draw_bitmap(
      panel_, area.x1, area.y1, area.x2 + 1, area.y2 + 1, colors);
  if (!first_draw_logged_) {
    if (error == ESP_OK) {
      log7B("first LVGL framebuffer flush complete");
    } else {
      log7BError("first LVGL framebuffer flush", error);
    }
    first_draw_logged_ = true;
  }
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
