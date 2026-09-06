#pragma once

#include <cstdint>

namespace spotctl {

enum class SwipeDirection { Previous, Next };

struct SwipeAnimationPlan {
  int16_t offset_px;
  uint16_t outward_ms;
  uint16_t return_ms;
};

inline SwipeAnimationPlan swipeAnimationPlan(SwipeDirection direction) {
  constexpr int16_t kDistancePx = 22;
  const int16_t offset = direction == SwipeDirection::Next
                             ? static_cast<int16_t>(-kDistancePx)
                             : kDistancePx;
  return {offset, 70, 110};
}

struct ButtonAnimationPlan {
  int16_t pressed_translate_y_px;
  uint16_t press_ms;
  uint16_t release_ms;
};

inline ButtonAnimationPlan buttonAnimationPlan() { return {1, 70, 110}; }

} // namespace spotctl
