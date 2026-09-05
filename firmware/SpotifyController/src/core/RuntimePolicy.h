#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "Models.h"

namespace spotctl {

uint32_t pollIntervalMs(const PlaybackSnapshot &playback);
uint32_t interpolatedProgressMs(const PlaybackSnapshot &playback,
                                uint32_t now_ms);
uint32_t backoffMs(uint8_t attempt, uint32_t jitter_ms = 0);
bool playlistItemsBrowsable(bool owned, bool collaborative);
ErrorCategory classifySpotifyError(int http_status, const std::string &reason);

template <typename T> class PageWindow {
public:
  PageWindow(size_t page_size, size_t max_pages)
      : capacity_(page_size * max_pages) {}

  void append(const T &item) {
    items_.push_back(item);
    if (items_.size() > capacity_) {
      items_.erase(items_.begin(),
                   items_.begin() + static_cast<std::ptrdiff_t>(items_.size() -
                                                                capacity_));
    }
  }

  void append(const std::vector<T> &items) {
    for (const auto &item : items) {
      append(item);
    }
  }

  void clear() { items_.clear(); }
  const std::vector<T> &items() const { return items_; }

private:
  size_t capacity_;
  std::vector<T> items_;
};

} // namespace spotctl
