#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "core/AppState.h"
#include "core/RuntimePolicy.h"

namespace {

int failures = 0;

template <typename Actual, typename Expected>
void expectEqual(const Actual &actual, const Expected &expected,
                 const char *expression, int line) {
  if (actual == expected) {
    return;
  }
  std::cerr << "line " << line << ": " << expression << " failed\n";
  ++failures;
}

#define EXPECT_EQ(actual, expected)                                             \
  expectEqual((actual), (expected), #actual " == " #expected, __LINE__)

#define EXPECT_TRUE(value) EXPECT_EQ(static_cast<bool>(value), true)
#define EXPECT_FALSE(value) EXPECT_EQ(static_cast<bool>(value), false)

using spotctl::AppEvent;
using spotctl::AppEventType;
using spotctl::AppState;
using spotctl::ConnectionState;
using spotctl::ErrorCategory;
using spotctl::MediaItem;
using spotctl::PlaybackSnapshot;
using spotctl::Screen;

void pollIntervalTracksPlaybackState() {
  PlaybackSnapshot active;
  active.has_item = true;
  active.is_playing = true;
  EXPECT_EQ(spotctl::pollIntervalMs(active), 2000U);

  active.is_playing = false;
  EXPECT_EQ(spotctl::pollIntervalMs(active), 5000U);

  active.has_item = false;
  EXPECT_EQ(spotctl::pollIntervalMs(active), 15000U);
}

void progressInterpolationClampsToDuration() {
  PlaybackSnapshot playback;
  playback.has_item = true;
  playback.is_playing = true;
  playback.progress_ms = 19500;
  playback.item.duration_ms = 20000;
  playback.observed_at_ms = 1000;

  EXPECT_EQ(spotctl::interpolatedProgressMs(playback, 1250), 19750U);
  EXPECT_EQ(spotctl::interpolatedProgressMs(playback, 5000), 20000U);

  playback.is_playing = false;
  EXPECT_EQ(spotctl::interpolatedProgressMs(playback, 5000), 19500U);
}

void retryBackoffIsBounded() {
  const std::vector<uint32_t> expected{2000, 4000, 8000, 16000,
                                       32000, 60000, 60000};
  for (size_t attempt = 0; attempt < expected.size(); ++attempt) {
    EXPECT_EQ(spotctl::backoffMs(static_cast<uint8_t>(attempt)),
              expected[attempt]);
  }
  EXPECT_EQ(spotctl::backoffMs(2, 137), 8137U);
}

void playlistBrowsingHonorsSpotifyCapability() {
  EXPECT_TRUE(spotctl::playlistItemsBrowsable(true, false));
  EXPECT_TRUE(spotctl::playlistItemsBrowsable(false, true));
  EXPECT_FALSE(spotctl::playlistItemsBrowsable(false, false));
}

void pageWindowRetainsOnlyThreePages() {
  spotctl::PageWindow<int> window(20, 3);
  for (int value = 0; value < 80; ++value) {
    window.append(value);
  }
  EXPECT_EQ(window.items().size(), static_cast<size_t>(60));
  EXPECT_EQ(window.items().front(), 20);
  EXPECT_EQ(window.items().back(), 79);

  window.clear();
  EXPECT_TRUE(window.items().empty());
}

void reducerKeepsNetworkCallbacksAwayFromUiObjects() {
  AppState state;
  EXPECT_EQ(state.connection, ConnectionState::Disconnected);

  state = spotctl::reduce(state, AppEvent{AppEventType::WifiConnecting});
  EXPECT_EQ(state.connection, ConnectionState::Connecting);

  state = spotctl::reduce(state, AppEvent{AppEventType::WifiConnected});
  EXPECT_EQ(state.connection, ConnectionState::Online);

  AppEvent playback_event{AppEventType::PlaybackLoaded};
  playback_event.playback.has_item = true;
  playback_event.playback.item.title = "Midnight City";
  state = spotctl::reduce(state, playback_event);
  EXPECT_EQ(state.playback.item.title, std::string("Midnight City"));
  EXPECT_TRUE(state.playback.has_item);

  AppEvent navigation{AppEventType::Navigate};
  navigation.screen = Screen::Library;
  state = spotctl::reduce(state, navigation);
  EXPECT_EQ(state.screen, Screen::Library);

  AppEvent disconnected{AppEventType::WifiDisconnected};
  disconnected.message = "Offline";
  state = spotctl::reduce(state, disconnected);
  EXPECT_EQ(state.connection, ConnectionState::Disconnected);
  EXPECT_EQ(state.message, std::string("Offline"));
  EXPECT_TRUE(state.playback.has_item);
}

void spotifyErrorsMapToActionableCategories() {
  EXPECT_EQ(spotctl::classifySpotifyError(401, ""),
            ErrorCategory::Authorization);
  EXPECT_EQ(spotctl::classifySpotifyError(403, "PREMIUM_REQUIRED"),
            ErrorCategory::Capability);
  EXPECT_EQ(spotctl::classifySpotifyError(404, "NO_ACTIVE_DEVICE"),
            ErrorCategory::NoDevice);
  EXPECT_EQ(spotctl::classifySpotifyError(429, "QUOTA_EXCEEDED"),
            ErrorCategory::RateLimited);
  EXPECT_EQ(spotctl::classifySpotifyError(503, ""),
            ErrorCategory::Transient);
  EXPECT_EQ(spotctl::classifySpotifyError(400, ""),
            ErrorCategory::Permanent);
}

} // namespace

int main() {
  pollIntervalTracksPlaybackState();
  progressInterpolationClampsToDuration();
  retryBackoffIsBounded();
  playlistBrowsingHonorsSpotifyCapability();
  pageWindowRetainsOnlyThreePages();
  reducerKeepsNetworkCallbacksAwayFromUiObjects();
  spotifyErrorsMapToActionableCategories();

  if (failures != 0) {
    std::cerr << failures << " assertion(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "all core assertions passed\n";
  return EXIT_SUCCESS;
}

