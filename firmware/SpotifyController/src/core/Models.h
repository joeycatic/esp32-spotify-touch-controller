#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spotctl {

enum class MediaType { Track, Episode, Unknown };
enum class RepeatMode { Off, Context, Track };

struct MediaItem {
  std::string uri;
  std::string title;
  std::string subtitle;
  uint32_t duration_ms{0};
  std::string artwork_url;
  std::string spotify_url;
  MediaType type{MediaType::Unknown};
};

struct PlaybackDevice {
  std::string id;
  std::string name;
  std::string type;
  bool active{false};
  bool restricted{false};
  int volume_percent{-1};
};

struct PlaybackSnapshot {
  bool has_item{false};
  MediaItem item;
  bool is_playing{false};
  uint32_t progress_ms{0};
  uint32_t observed_at_ms{0};
  int volume_percent{-1};
  bool shuffle{false};
  RepeatMode repeat{RepeatMode::Off};
  PlaybackDevice device;
  std::string context_uri;
};

struct PlaylistSummary {
  std::string id;
  std::string uri;
  std::string name;
  std::string owner;
  std::string artwork_url;
  std::string thumbnail_url;
  bool collaborative{false};
  bool owned{false};
  bool items_browsable{false};
};

struct TrackSummary {
  std::string uri;
  std::string title;
  std::string artists;
  uint32_t duration_ms{0};
  std::string artwork_url;
  std::string thumbnail_url;
  uint32_t position{0};
};

enum class ErrorCategory {
  None,
  Authorization,
  Capability,
  NoDevice,
  RateLimited,
  Transient,
  Permanent,
};

struct SpotifyError {
  int http_status{0};
  std::string reason;
  uint32_t retry_after_ms{0};
  ErrorCategory category{ErrorCategory::None};
  std::string user_message;
};

} // namespace spotctl

