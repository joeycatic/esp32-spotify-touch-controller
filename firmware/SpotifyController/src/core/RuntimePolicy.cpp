#include "RuntimePolicy.h"

#include <algorithm>
#include <limits>

namespace spotctl {

uint32_t pollIntervalMs(const PlaybackSnapshot &playback) {
  if (!playback.has_item) {
    return 15000;
  }
  return playback.is_playing ? 2000 : 5000;
}

uint32_t interpolatedProgressMs(const PlaybackSnapshot &playback,
                                uint32_t now_ms) {
  uint64_t progress = playback.progress_ms;
  if (playback.has_item && playback.is_playing &&
      now_ms >= playback.observed_at_ms) {
    progress += static_cast<uint64_t>(now_ms - playback.observed_at_ms);
  }
  return static_cast<uint32_t>(
      std::min<uint64_t>(progress, playback.item.duration_ms));
}

uint32_t backoffMs(uint8_t attempt, uint32_t jitter_ms) {
  const uint8_t bounded_attempt = std::min<uint8_t>(attempt, 5);
  const uint32_t base = std::min<uint32_t>(2000U << bounded_attempt, 60000U);
  const uint64_t total = static_cast<uint64_t>(base) + jitter_ms;
  return static_cast<uint32_t>(
      std::min<uint64_t>(total, std::numeric_limits<uint32_t>::max()));
}

bool playlistItemsBrowsable(bool owned, bool collaborative) {
  return owned || collaborative;
}

ErrorCategory classifySpotifyError(int http_status, const std::string &) {
  if (http_status == 401) {
    return ErrorCategory::Authorization;
  }
  if (http_status == 403) {
    return ErrorCategory::Capability;
  }
  if (http_status == 404) {
    return ErrorCategory::NoDevice;
  }
  if (http_status == 429) {
    return ErrorCategory::RateLimited;
  }
  if (http_status <= 0 || http_status >= 500) {
    return ErrorCategory::Transient;
  }
  return ErrorCategory::Permanent;
}

bool commandAccepted(bool service_running, bool wifi_connected,
                     bool rate_limited) {
  return service_running && wifi_connected && !rate_limited;
}

bool networkWorkNeedsArtworkRelease(NetworkWork work) {
  return work == NetworkWork::SpotifyApi || work == NetworkWork::PlaybackPoll;
}

void applyOptimisticPlayback(PlaybackSnapshot &playback,
                             PlaybackMutation mutation) {
  switch (mutation) {
  case PlaybackMutation::TogglePlaying:
    playback.is_playing = !playback.is_playing;
    break;
  case PlaybackMutation::ToggleShuffle:
    playback.shuffle = !playback.shuffle;
    break;
  case PlaybackMutation::CycleRepeat:
    playback.repeat = playback.repeat == RepeatMode::Off
                          ? RepeatMode::Context
                          : (playback.repeat == RepeatMode::Context
                                 ? RepeatMode::Track
                                 : RepeatMode::Off);
    break;
  }
}

bool rowIntersectsViewport(int32_t row_top, int32_t row_bottom,
                           int32_t viewport_top, int32_t viewport_bottom) {
  if (row_bottom < row_top || viewport_bottom < viewport_top) {
    return false;
  }
  return row_bottom >= viewport_top && row_top <= viewport_bottom;
}

} // namespace spotctl
