#include "NetworkService.h"

#include <WiFi.h>
#include <time.h>

#include "../core/RuntimePolicy.h"

namespace spotctl {

namespace {
constexpr size_t kMaximumQueuedMessages = 16;

bool due(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}
} // namespace

NetworkService::NetworkService(ConfigStore &store)
    : store_(store), spotify_(store) {}

bool NetworkService::begin(const DeviceConfig &config) {
  if (running_) {
    return true;
  }
  config_ = config;
  mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) {
    return false;
  }
  running_ = xTaskCreatePinnedToCore(taskEntry, "spotify-net", 16384, this, 1,
                                    &task_, 0) == pdPASS;
  return running_;
}

bool NetworkService::enqueue(const UiCommand &command) {
  if (!running_ || mutex_ == nullptr ||
      xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }
  if (commands_.size() >= kMaximumQueuedMessages) {
    commands_.pop_front();
  }
  commands_.push_back(command);
  xSemaphoreGive(mutex_);
  return true;
}

bool NetworkService::pollEvent(NetworkEvent &event) {
  if (mutex_ == nullptr || xSemaphoreTake(mutex_, 0) != pdTRUE) {
    return false;
  }
  if (events_.empty()) {
    xSemaphoreGive(mutex_);
    return false;
  }
  event = std::move(events_.front());
  events_.pop_front();
  xSemaphoreGive(mutex_);
  return true;
}

bool NetworkService::popCommand(UiCommand &command) {
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }
  if (commands_.empty()) {
    xSemaphoreGive(mutex_);
    return false;
  }
  command = std::move(commands_.front());
  commands_.pop_front();
  xSemaphoreGive(mutex_);
  return true;
}

void NetworkService::pushEvent(NetworkEvent event) {
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
    return;
  }
  if (events_.size() >= kMaximumQueuedMessages) {
    events_.pop_front();
  }
  events_.push_back(std::move(event));
  xSemaphoreGive(mutex_);
}

void NetworkService::taskEntry(void *context) {
  static_cast<NetworkService *>(context)->run();
}

bool NetworkService::syncClock() {
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  const uint32_t started = millis();
  while (millis() - started < 15000) {
    time_t now = 0;
    time(&now);
    if (now > 1700000000) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

void NetworkService::run() {
  spotify_.begin(config_);
  uint8_t startup_attempt = 0;
  while (!syncClock()) {
    NetworkEvent event{NetworkEventType::Error};
    event.error.category = ErrorCategory::Transient;
    event.error.user_message = "Could not synchronize time for secure Spotify access";
    pushEvent(std::move(event));
    vTaskDelay(pdMS_TO_TICKS(backoffMs(startup_attempt++)));
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  SpotifyError error;
  std::string account_id;
  startup_attempt = 0;
  while (!spotify_.refreshAccessToken(error) ||
         !spotify_.loadAccountId(account_id, error)) {
    NetworkEvent event{error.category == ErrorCategory::Authorization
                           ? NetworkEventType::AuthorizationRequired
                           : NetworkEventType::Error};
    event.error = error;
    pushEvent(std::move(event));
    if (error.category == ErrorCategory::Authorization) {
      for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(backoffMs(startup_attempt++)));
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  pushEvent(NetworkEvent{NetworkEventType::Authorized});
  next_poll_ms_ = millis();

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    const uint32_t now = millis();
    UiCommand command{UiCommandType::TogglePlay};
    if (popCommand(command)) {
      if (blocked_until_ms_ == 0 || due(now, blocked_until_ms_)) {
        process(command);
      } else {
        NetworkEvent event{NetworkEventType::Error};
        event.error.category = ErrorCategory::RateLimited;
        event.error.retry_after_ms = blocked_until_ms_ - now;
        event.error.user_message = "Spotify is rate limiting requests";
        pushEvent(std::move(event));
      }
    }
    if ((blocked_until_ms_ == 0 || due(now, blocked_until_ms_)) &&
        due(now, next_poll_ms_)) {
      blocked_until_ms_ = 0;
      pollPlayback();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void NetworkService::scheduleAfterRequest(bool success,
                                          const SpotifyError &error) {
  const uint32_t now = millis();
  if (success) {
    transient_attempt_ = 0;
    next_poll_ms_ = now + 400;
    return;
  }
  if (error.category == ErrorCategory::RateLimited) {
    const uint32_t wait_ms = error.retry_after_ms > 0 ? error.retry_after_ms : 30000;
    blocked_until_ms_ = now + wait_ms;
    next_poll_ms_ = blocked_until_ms_;
  } else if (error.category == ErrorCategory::Transient) {
    next_poll_ms_ =
        now + backoffMs(transient_attempt_++, static_cast<uint32_t>(esp_random() % 500));
  } else {
    next_poll_ms_ = now + 15000;
  }
}

void NetworkService::publishFailure(const SpotifyError &error) {
  NetworkEvent event{error.category == ErrorCategory::Authorization
                         ? NetworkEventType::AuthorizationRequired
                         : NetworkEventType::Error};
  event.error = error;
  pushEvent(std::move(event));
  scheduleAfterRequest(false, error);
}

void NetworkService::pollPlayback() {
  SpotifyError error;
  PlaybackSnapshot snapshot;
  if (!spotify_.getPlayback(snapshot, error)) {
    publishFailure(error);
    return;
  }
  const lv_img_dsc_t *image = nullptr;
  if (snapshot.has_item && snapshot.item.uri != artwork_uri_) {
    artwork_uri_ = snapshot.item.uri;
    image = artwork_.load(snapshot.item.artwork_url);
  }
  playback_ = snapshot;
  NetworkEvent event{NetworkEventType::Playback};
  event.playback = snapshot;
  event.artwork = image;
  pushEvent(std::move(event));
  transient_attempt_ = 0;
  next_poll_ms_ = millis() + pollIntervalMs(playback_);
}

void NetworkService::process(const UiCommand &command) {
  SpotifyError error;
  bool success = false;
  switch (command.type) {
  case UiCommandType::TogglePlay:
    success = playback_.is_playing ? spotify_.pause(error) : spotify_.resume(error);
    break;
  case UiCommandType::Previous:
    success = spotify_.previous(error);
    break;
  case UiCommandType::Next:
    success = spotify_.next(error);
    break;
  case UiCommandType::Seek:
    success = spotify_.seek(command.value, error);
    break;
  case UiCommandType::SetVolume:
    success = spotify_.setVolume(static_cast<uint8_t>(command.value), error);
    break;
  case UiCommandType::ToggleShuffle:
    success = spotify_.setShuffle(!playback_.shuffle, error);
    break;
  case UiCommandType::CycleRepeat: {
    const RepeatMode next = playback_.repeat == RepeatMode::Off
                                ? RepeatMode::Context
                                : (playback_.repeat == RepeatMode::Context
                                       ? RepeatMode::Track
                                       : RepeatMode::Off);
    success = spotify_.setRepeat(next, error);
    break;
  }
  case UiCommandType::LoadPlaylists:
  case UiCommandType::LoadMorePlaylists: {
    const bool replace = command.type == UiCommandType::LoadPlaylists;
    if (replace) {
      playlist_offset_ = 0;
    } else if (!more_playlists_) {
      return;
    }
    SpotifyPage<PlaylistSummary> page;
    success = spotify_.getPlaylists(playlist_offset_, page, error);
    if (success) {
      NetworkEvent event{NetworkEventType::Playlists};
      event.playlists = std::move(page.items);
      event.replace = replace;
      event.has_more = page.has_more;
      more_playlists_ = page.has_more;
      playlist_offset_ += 20;
      pushEvent(std::move(event));
    }
    break;
  }
  case UiCommandType::LoadPlaylist:
  case UiCommandType::LoadMoreTracks: {
    const bool replace = command.type == UiCommandType::LoadPlaylist;
    if (replace) {
      selected_playlist_id_ = command.id;
      selected_context_uri_ = command.uri;
      selected_title_ = command.title;
      selected_liked_ = false;
      track_offset_ = 0;
    } else if (!more_tracks_) {
      return;
    }
    SpotifyPage<TrackSummary> page;
    success = selected_liked_
                  ? spotify_.getSavedTracks(track_offset_, page, error)
                  : spotify_.getPlaylistItems(selected_playlist_id_, track_offset_,
                                              page, error);
    if (success) {
      NetworkEvent event{NetworkEventType::Tracks};
      event.tracks = std::move(page.items);
      event.replace = replace;
      event.has_more = page.has_more;
      event.title = selected_title_;
      event.liked = selected_liked_;
      more_tracks_ = page.has_more;
      track_offset_ += 20;
      pushEvent(std::move(event));
    }
    break;
  }
  case UiCommandType::LoadLikedSongs: {
    selected_playlist_id_.clear();
    selected_context_uri_.clear();
    selected_title_ = "Liked Songs";
    selected_liked_ = true;
    track_offset_ = 0;
    SpotifyPage<TrackSummary> page;
    success = spotify_.getSavedTracks(0, page, error);
    if (success) {
      NetworkEvent event{NetworkEventType::Tracks};
      event.tracks = std::move(page.items);
      event.replace = true;
      event.has_more = page.has_more;
      event.liked = true;
      event.title = selected_title_;
      more_tracks_ = page.has_more;
      track_offset_ = 20;
      pushEvent(std::move(event));
    }
    break;
  }
  case UiCommandType::PlayPlaylist:
    success = spotify_.playContext(command.uri, 0, error);
    break;
  case UiCommandType::PlayTrack:
    success = command.liked ? spotify_.playUris(command.uris, error)
                            : spotify_.playContext(selected_context_uri_,
                                                   command.position, error);
    break;
  case UiCommandType::LoadDevices: {
    std::vector<PlaybackDevice> devices;
    success = spotify_.getDevices(devices, error);
    if (success) {
      NetworkEvent event{NetworkEventType::Devices};
      event.devices = std::move(devices);
      pushEvent(std::move(event));
    }
    break;
  }
  case UiCommandType::TransferDevice:
    success = spotify_.transferPlayback(command.id, error);
    break;
  }
  if (!success) {
    publishFailure(error);
  } else {
    scheduleAfterRequest(true, error);
  }
}

} // namespace spotctl
