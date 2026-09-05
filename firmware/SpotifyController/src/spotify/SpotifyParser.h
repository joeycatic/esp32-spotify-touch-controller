#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/Models.h"

namespace spotctl {

template <typename T> struct SpotifyPage {
  std::vector<T> items;
  uint32_t total{0};
  bool has_more{false};
};

bool parsePlayback(const std::string &json, uint32_t observed_at_ms,
                   PlaybackSnapshot &playback);
bool parseDevices(const std::string &json,
                  std::vector<PlaybackDevice> &devices);
bool parsePlaylists(const std::string &json,
                    const std::string &current_account_id,
                    SpotifyPage<PlaylistSummary> &page);
bool parsePlaylistItems(const std::string &json, uint32_t page_offset,
                        SpotifyPage<TrackSummary> &page);
bool parseSavedTracks(const std::string &json, uint32_t page_offset,
                      SpotifyPage<TrackSummary> &page);
SpotifyError parseSpotifyError(int status, const std::string &json,
                               uint32_t retry_after_seconds = 0);

} // namespace spotctl

