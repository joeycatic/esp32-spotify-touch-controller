#pragma once

#include <cstdint>

namespace spotctl {

struct UiLayoutMetrics {
  uint16_t width;
  uint16_t height;
  uint16_t top_bar_height;
  uint16_t bottom_nav_height;
  uint16_t player_art_size;
  uint16_t minimum_touch_target;
};

inline constexpr UiLayoutMetrics kUiLayout{1024, 600, 64, 72, 400, 64};

} // namespace spotctl
