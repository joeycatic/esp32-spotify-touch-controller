#pragma once

#include <lvgl.h>

#include <cstdint>
#include <memory>

namespace spotctl {

// An immutable decoded image. Shared ownership keeps the PSRAM pixels alive
// until both the network event and LVGL UI have released the frame.
struct ArtworkFrame {
  ArtworkFrame() = default;
  ArtworkFrame(const ArtworkFrame &) = delete;
  ArtworkFrame &operator=(const ArtworkFrame &) = delete;

  uint16_t *pixels{nullptr};
  lv_img_dsc_t image{};
  uint16_t width{0};
  uint16_t height{0};

  ~ArtworkFrame();
};

using ArtworkHandle = std::shared_ptr<ArtworkFrame>;

} // namespace spotctl
