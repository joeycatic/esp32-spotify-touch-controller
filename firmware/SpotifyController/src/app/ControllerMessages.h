#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/Models.h"
#include "../spotify/ArtworkFrame.h"

namespace spotctl {

enum class UiCommandType {
  TogglePlay,
  Previous,
  Next,
  Seek,
  SetVolume,
  ToggleShuffle,
  CycleRepeat,
  LoadPlaylists,
  LoadMorePlaylists,
  LoadPlaylist,
  LoadMoreTracks,
  LoadLikedSongs,
  PlayPlaylist,
  PlayTrack,
  LoadDevices,
  TransferDevice,
};

struct UiCommand {
  UiCommandType type;
  std::string id;
  std::string uri;
  std::string title;
  uint32_t position{0};
  uint32_t value{0};
  bool liked{false};
  std::vector<std::string> uris;
};

enum class NetworkEventType {
  Authorized,
  AuthorizationRequired,
  Playback,
  Playlists,
  Tracks,
  Devices,
  Artwork,
  Error,
  Status,
};

struct NetworkEvent {
  NetworkEventType type;
  PlaybackSnapshot playback;
  std::vector<PlaylistSummary> playlists;
  std::vector<TrackSummary> tracks;
  std::vector<PlaybackDevice> devices;
  SpotifyError error;
  ArtworkHandle artwork;
  // Row this artwork belongs to: playlist id, or track uri.
  std::string key;
  bool replace{false};
  bool has_more{false};
  bool liked{false};
  std::string title;
  std::string message;
};

} // namespace spotctl
