#pragma once

#include <NetworkClientSecure.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../core/Models.h"
#include "../storage/ConfigStore.h"
#include "SpotifyParser.h"

namespace spotctl {

class SpotifyClient {
public:
  explicit SpotifyClient(ConfigStore &store);

  void begin(const DeviceConfig &config);
  void setDiagnosticStream(Stream &stream) { diagnostic_ = &stream; }
  bool refreshAccessToken(SpotifyError &error);
  bool loadAccountId(std::string &account_id, SpotifyError &error);
  bool getPlayback(PlaybackSnapshot &playback, SpotifyError &error);
  bool getDevices(std::vector<PlaybackDevice> &devices, SpotifyError &error);
  bool getPlaylists(uint32_t offset, SpotifyPage<PlaylistSummary> &page,
                    SpotifyError &error);
  bool getPlaylistItems(const std::string &playlist_id, uint32_t offset,
                        SpotifyPage<TrackSummary> &page, SpotifyError &error);
  bool getSavedTracks(uint32_t offset, SpotifyPage<TrackSummary> &page,
                      SpotifyError &error);

  bool resume(SpotifyError &error);
  bool pause(SpotifyError &error);
  bool next(SpotifyError &error);
  bool previous(SpotifyError &error);
  bool seek(uint32_t position_ms, SpotifyError &error);
  bool setVolume(uint8_t percent, SpotifyError &error);
  bool setShuffle(bool enabled, SpotifyError &error);
  bool setRepeat(RepeatMode mode, SpotifyError &error);
  bool transferPlayback(const std::string &device_id, SpotifyError &error);
  bool playContext(const std::string &context_uri, uint32_t position,
                   SpotifyError &error);
  bool playUris(const std::vector<std::string> &uris, SpotifyError &error);

  const std::string &accountId() const { return account_id_; }

private:
  struct HttpResponse {
    int status{0};
    std::string body;
    uint32_t retry_after_seconds{0};
  };

  HttpResponse requestRaw(const char *method, const std::string &url,
                          const std::string &body, const char *content_type,
                          bool authenticated);
  bool apiRequest(const char *method, const std::string &path,
                  const std::string &body, HttpResponse &response,
                  SpotifyError &error);
  bool command(const char *method, const std::string &path,
               const std::string &body, SpotifyError &error);
  bool ensureAccessToken(SpotifyError &error);

  ConfigStore &store_;
  NetworkClientSecure secure_client_;
  std::string client_id_;
  std::string refresh_token_;
  std::string access_token_;
  std::string account_id_;
  uint32_t refresh_at_ms_{0};
  Stream *diagnostic_{nullptr};
};

} // namespace spotctl
