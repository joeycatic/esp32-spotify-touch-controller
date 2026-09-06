#include "NetworkService.h"

#include <WiFi.h>
#include <time.h>

#include <algorithm>

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
  const bool wifi_connected = WiFi.status() == WL_CONNECTED;
  if (!commandAccepted(running_, wifi_connected,
                       !accepting_commands_.load(std::memory_order_acquire)) ||
      mutex_ == nullptr ||
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

void NetworkService::resetThumbnailRequests() {
  if (mutex_ == nullptr ||
      xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  thumbnail_requests_.clear();
  xSemaphoreGive(mutex_);
}

bool NetworkService::requestThumbnail(const std::string &key,
                                      const std::string &url) {
  if (!running_ || mutex_ == nullptr ||
      xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }
  const bool queued = thumbnail_requests_.push(key, url);
  xSemaphoreGive(mutex_);
  return queued;
}

bool NetworkService::popThumbnail(std::string &key, std::string &url) {
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }
  const bool popped = thumbnail_requests_.pop(key, url);
  xSemaphoreGive(mutex_);
  return popped;
}

void NetworkService::serviceThumbnails() {
  std::string key;
  std::string url;
  while (popThumbnail(key, url)) {
    // Cached covers are answered without touching the network, so scrolling
    // back over rows already seen costs nothing.
    if (const ArtworkHandle *cached = thumbnails_.get(url)) {
      const ArtworkHandle frame = *cached;
      if (frame) {
        NetworkEvent event{NetworkEventType::Artwork};
        event.key = key;
        event.artwork = frame;
        pushEvent(std::move(event));
      }
      continue;
    }
    const ArtworkHandle frame = artwork_.loadThumbnail(url);
    // An empty handle is stored too: a cover that failed is remembered as
    // failed rather than retried on every scroll.
    thumbnails_.put(url, frame);
    if (frame) {
      NetworkEvent event{NetworkEventType::Artwork};
      event.key = key;
      event.artwork = frame;
      pushEvent(std::move(event));
    }
    // One download per pass keeps commands and playback polling responsive.
    return;
  }
  // The batch is done. Holding the connection open past it would keep ~50KB of
  // internal heap out of reach of the API's TLS handshakes.
  artwork_.releaseConnection();
}

void NetworkService::prepareForNetworkWork(NetworkWork work) {
  if (networkWorkNeedsArtworkRelease(work)) {
    // A kept-alive artwork TLS connection reserves roughly 50 KB of internal
    // heap. Spotify's API needs that block for its own TLS handshake.
    artwork_.releaseConnection();
  }
}

void NetworkService::clearCommands() {
  if (mutex_ == nullptr ||
      xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  commands_.clear();
  thumbnail_requests_.clear();
  xSemaphoreGive(mutex_);
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
  if (event.type == NetworkEventType::Playback) {
    events_.erase(std::remove_if(events_.begin(), events_.end(),
                                 [](const NetworkEvent &queued) {
                                   return queued.type ==
                                          NetworkEventType::Playback;
                                 }),
                  events_.end());
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
      accepting_commands_.store(false, std::memory_order_release);
      clearCommands();
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
  accepting_commands_.store(true, std::memory_order_release);
  next_poll_ms_ = millis();

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      accepting_commands_.store(false, std::memory_order_release);
      clearCommands();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    const uint32_t now = millis();
    accepting_commands_.store(
        blocked_until_ms_ == 0 || due(now, blocked_until_ms_),
        std::memory_order_release);
    UiCommand command{UiCommandType::TogglePlay};
    bool handled_command = false;
    if (popCommand(command)) {
      handled_command = true;
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
    } else if (!handled_command &&
               (blocked_until_ms_ == 0 || due(now, blocked_until_ms_))) {
      // Lowest priority: covers are fetched only in passes where nothing the
      // user is waiting on is due.
      serviceThumbnails();
    }
    if (authorization_required_) {
      for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void NetworkService::scheduleAfterRequest(bool success,
                                          const SpotifyError &error) {
  const uint32_t now = millis();
  if (success) {
    transient_attempt_ = 0;
    accepting_commands_.store(WiFi.status() == WL_CONNECTED,
                              std::memory_order_release);
    next_poll_ms_ = now + 400;
    return;
  }
  if (error.category == ErrorCategory::RateLimited) {
    accepting_commands_.store(false, std::memory_order_release);
    clearCommands();
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
  if (error.category == ErrorCategory::Authorization) {
    authorization_required_ = true;
    accepting_commands_.store(false, std::memory_order_release);
    clearCommands();
  }
  scheduleAfterRequest(false, error);
}

void NetworkService::pollPlayback() {
  prepareForNetworkWork(NetworkWork::PlaybackPoll);
  SpotifyError error;
  PlaybackSnapshot snapshot;
  if (!spotify_.getPlayback(snapshot, error)) {
    publishFailure(error);
    return;
  }
  ArtworkHandle image;
  if (snapshot.has_item && snapshot.item.uri != artwork_uri_) {
    if (snapshot.item.artwork_url.empty()) {
      artwork_uri_ = snapshot.item.uri;
    } else {
      image = artwork_.load(snapshot.item.artwork_url);
      if (image) {
        artwork_uri_ = snapshot.item.uri;
      }
    }
  } else if (!snapshot.has_item) {
    artwork_uri_.clear();
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
  prepareForNetworkWork(NetworkWork::SpotifyApi);
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
    bool publish_playback = false;
    switch (command.type) {
    case UiCommandType::TogglePlay:
      applyOptimisticPlayback(playback_, PlaybackMutation::TogglePlaying);
      publish_playback = true;
      break;
    case UiCommandType::Seek:
      playback_.progress_ms = command.value;
      playback_.observed_at_ms = millis();
      publish_playback = true;
      break;
    case UiCommandType::SetVolume:
      playback_.device.volume_percent =
          static_cast<uint8_t>(std::min<uint32_t>(command.value, 100));
      playback_.volume_percent = playback_.device.volume_percent;
      publish_playback = true;
      break;
    case UiCommandType::ToggleShuffle:
      applyOptimisticPlayback(playback_, PlaybackMutation::ToggleShuffle);
      publish_playback = true;
      break;
    case UiCommandType::CycleRepeat:
      applyOptimisticPlayback(playback_, PlaybackMutation::CycleRepeat);
      publish_playback = true;
      break;
    default:
      break;
    }
    if (publish_playback) {
      NetworkEvent event{NetworkEventType::Playback};
      event.playback = playback_;
      pushEvent(std::move(event));
    }
    scheduleAfterRequest(true, error);
  }
}

} // namespace spotctl
