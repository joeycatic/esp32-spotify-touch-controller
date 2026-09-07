#pragma once

#include <cstdint>

#include "../board/BoardProfile.h"

namespace spotctl {

struct UiLayoutMetrics {
  uint16_t width;
  uint16_t height;
  uint16_t top_bar_height;
  uint16_t bottom_nav_height;
  uint16_t player_art_size;
  uint16_t minimum_touch_target;
};

constexpr UiLayoutMetrics layoutFor(BoardProfile profile) {
  return profile == BoardProfile::Wide7B
             ? UiLayoutMetrics{1024, 600, 64, 72, 400, 64}
             : UiLayoutMetrics{240, 320, 20, 0, 184, 40};
}

} // namespace spotctl
