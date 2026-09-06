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
bool commandAccepted(bool service_running, bool wifi_connected,
                     bool rate_limited);

enum class NetworkWork { SpotifyApi, PlaybackPoll, Thumbnail };
bool networkWorkNeedsArtworkRelease(NetworkWork work);

class PlaylistLoadState {
public:
  bool shouldRequest(size_t cached_playlist_count) const {
    return cached_playlist_count == 0 && !request_pending_;
  }
  void markRequested() { request_pending_ = true; }
  void markLoaded() { request_pending_ = false; }
  void markFailed() { request_pending_ = false; }

private:
  bool request_pending_{false};
};

enum class PlaybackMutation { TogglePlaying, ToggleShuffle, CycleRepeat };
void applyOptimisticPlayback(PlaybackSnapshot &playback,
                             PlaybackMutation mutation);

// Whether a row is on screen, in absolute display coordinates. Covers are
// fetched only for rows that pass this, so scrolling past a long list never
// spends bandwidth on pixels nobody sees. Rows are not a uniform height, so
// this compares real geometry rather than assuming a pitch.
bool rowIntersectsViewport(int32_t row_top, int32_t row_bottom,
                           int32_t viewport_top, int32_t viewport_bottom);

// Bounded most-recently-used cache. A stored empty value is a remembered
// failure, which stops a broken cover from being retried on every scroll.
template <typename T> class LruCache {
public:
  explicit LruCache(size_t capacity)
      : capacity_(capacity == 0 ? 1 : capacity) {}

  // The returned pointer is invalidated by the next put(); copy what you need.
  const T *get(const std::string &key) {
    for (size_t index = 0; index < entries_.size(); ++index) {
      if (entries_[index].key != key) {
        continue;
      }
      if (index != 0) {
        Entry entry = std::move(entries_[index]);
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
        entries_.push_front(std::move(entry));
      }
      return &entries_.front().value;
    }
    return nullptr;
  }

  bool contains(const std::string &key) const {
    for (const Entry &entry : entries_) {
      if (entry.key == key) {
        return true;
      }
    }
    return false;
  }

  void put(const std::string &key, T value) {
    for (size_t index = 0; index < entries_.size(); ++index) {
      if (entries_[index].key == key) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
        break;
      }
    }
    entries_.push_front(Entry{key, std::move(value)});
    while (entries_.size() > capacity_) {
      entries_.pop_back();
    }
  }

  void clear() { entries_.clear(); }
  size_t size() const { return entries_.size(); }

private:
  struct Entry {
    std::string key;
    T value;
  };

  size_t capacity_;
  std::deque<Entry> entries_;
};

// Pending thumbnail fetches, in request order. Bounded and duplicate-free, and
// cleared whenever the visible window moves so abandoned rows are never
// fetched.
class ThumbnailQueue {
public:
  explicit ThumbnailQueue(size_t capacity) : capacity_(capacity) {}

  bool push(const std::string &key, const std::string &url) {
    if (key.empty() || url.empty() || entries_.size() >= capacity_) {
      return false;
    }
    for (const Entry &entry : entries_) {
      if (entry.key == key) {
        return false;
      }
    }
    entries_.push_back(Entry{key, url});
    return true;
  }

  bool pop(std::string &key, std::string &url) {
    if (entries_.empty()) {
      return false;
    }
    key = entries_.front().key;
    url = entries_.front().url;
    entries_.pop_front();
    return true;
  }

  void clear() { entries_.clear(); }
  size_t size() const { return entries_.size(); }

private:
  struct Entry {
    std::string key;
    std::string url;
  };

  size_t capacity_;
  std::deque<Entry> entries_;
};

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
