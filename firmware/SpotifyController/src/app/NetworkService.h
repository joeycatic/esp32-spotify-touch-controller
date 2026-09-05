#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <deque>
#include <string>

#include "../spotify/ArtworkManager.h"
#include "../spotify/SpotifyClient.h"
#include "../storage/ConfigStore.h"
#include "ControllerMessages.h"

namespace spotctl {

class NetworkService {
public:
  explicit NetworkService(ConfigStore &store);

  bool begin(const DeviceConfig &config);
  bool enqueue(const UiCommand &command);
  bool pollEvent(NetworkEvent &event);
  bool running() const { return running_; }

private:
  static void taskEntry(void *context);
  void run();
  bool syncClock();
  bool popCommand(UiCommand &command);
  void clearCommands();
  void pushEvent(NetworkEvent event);
  void pollPlayback();
  void process(const UiCommand &command);
  void publishFailure(const SpotifyError &error);
  void scheduleAfterRequest(bool success, const SpotifyError &error);

  ConfigStore &store_;
  SpotifyClient spotify_;
  ArtworkManager artwork_;
  DeviceConfig config_;
  SemaphoreHandle_t mutex_{nullptr};
  std::deque<UiCommand> commands_;
  std::deque<NetworkEvent> events_;
  TaskHandle_t task_{nullptr};
  volatile bool running_{false};
  std::atomic<bool> accepting_commands_{false};
  PlaybackSnapshot playback_;
  std::string artwork_uri_;
  std::string selected_playlist_id_;
  std::string selected_context_uri_;
  std::string selected_title_;
  bool selected_liked_{false};
  uint32_t playlist_offset_{0};
  uint32_t track_offset_{0};
  bool more_playlists_{false};
  bool more_tracks_{false};
  bool authorization_required_{false};
  uint32_t next_poll_ms_{0};
  uint32_t blocked_until_ms_{0};
  uint8_t transient_attempt_{0};
};

} // namespace spotctl
