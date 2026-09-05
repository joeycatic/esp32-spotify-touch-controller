#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "core/AppState.h"
#include "core/RuntimePolicy.h"
#include "provision/ProvisioningValidation.h"
#include "spotify/SpotifyParser.h"

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

void provisioningValidationMatchesTheDesktopUtility() {
  spotctl::ProvisioningFields valid{
      "Studio WiFi", "correct horse battery staple",
      "0123456789abcdef0123456789abcdef", "AQD-refresh-token"};
  EXPECT_EQ(spotctl::validateProvisioning(valid), std::string());

  valid.ssid = std::string(33, 'x');
  EXPECT_EQ(spotctl::validateProvisioning(valid),
            std::string("invalid_wifi_ssid"));

  valid.ssid = "Studio WiFi";
  valid.password = "short";
  EXPECT_EQ(spotctl::validateProvisioning(valid),
            std::string("invalid_wifi_password"));

  valid.password = "password";
  valid.client_id = "tiny";
  EXPECT_EQ(spotctl::validateProvisioning(valid),
            std::string("invalid_client_id"));

  valid.client_id = "0123456789abcdef0123456789abcdef";
  valid.refresh_token.clear();
  EXPECT_EQ(spotctl::validateProvisioning(valid),
            std::string("invalid_refresh_token"));
}

void playbackParserHandlesTracksAndEpisodes() {
  const std::string track_json = R"json({
    "is_playing":true,"progress_ms":42000,"shuffle_state":true,
    "repeat_state":"context","context":{"uri":"spotify:playlist:list1"},
    "device":{"id":"dev1","name":"Kitchen","type":"Speaker","is_active":true,
      "is_restricted":false,"volume_percent":67},
    "currently_playing_type":"track",
    "item":{"uri":"spotify:track:t1","name":"Midnight City","duration_ms":244000,
      "artists":[{"name":"M83"}],
      "album":{"images":[{"url":"large.jpg","width":640},{"url":"medium.jpg","width":300}]},
      "external_urls":{"spotify":"https://open.spotify.com/track/t1"}}
  })json";
  PlaybackSnapshot playback;
  EXPECT_TRUE(spotctl::parsePlayback(track_json, 1234, playback));
  EXPECT_TRUE(playback.has_item);
  EXPECT_EQ(playback.item.title, std::string("Midnight City"));
  EXPECT_EQ(playback.item.subtitle, std::string("M83"));
  EXPECT_EQ(playback.item.artwork_url, std::string("medium.jpg"));
  EXPECT_EQ(playback.progress_ms, 42000U);
  EXPECT_EQ(playback.observed_at_ms, 1234U);
  EXPECT_EQ(playback.device.name, std::string("Kitchen"));
  EXPECT_EQ(playback.repeat, spotctl::RepeatMode::Context);

  const std::string episode_json = R"json({
    "is_playing":false,"progress_ms":12,"currently_playing_type":"episode",
    "item":{"uri":"spotify:episode:e1","name":"The Test Episode","duration_ms":9000,
      "show":{"name":"A Useful Podcast"},
      "images":[{"url":"episode.jpg","width":300}],
      "external_urls":{"spotify":"https://open.spotify.com/episode/e1"}}
  })json";
  EXPECT_TRUE(spotctl::parsePlayback(episode_json, 99, playback));
  EXPECT_EQ(playback.item.type, spotctl::MediaType::Episode);
  EXPECT_EQ(playback.item.subtitle, std::string("A Useful Podcast"));
}

void playlistParserAppliesOwnershipRestriction() {
  const std::string json = R"json({"total":2,"next":null,"items":[
    {"id":"mine","uri":"spotify:playlist:mine","name":"My Mix","collaborative":false,
      "owner":{"account_id":"account-1","display_name":"Joey"},
      "images":[{"url":"mine.jpg","width":300}]},
    {"id":"theirs","uri":"spotify:playlist:theirs","name":"Their Mix","collaborative":false,
      "owner":{"account_id":"account-2","display_name":"Alex"},"images":[]}
  ]})json";
  spotctl::SpotifyPage<spotctl::PlaylistSummary> page;
  EXPECT_TRUE(spotctl::parsePlaylists(json, "account-1", page));
  EXPECT_EQ(page.items.size(), static_cast<size_t>(2));
  EXPECT_TRUE(page.items[0].owned);
  EXPECT_TRUE(page.items[0].items_browsable);
  EXPECT_FALSE(page.items[1].owned);
  EXPECT_FALSE(page.items[1].items_browsable);
  EXPECT_FALSE(page.has_more);
}

void trackParserSkipsUnavailableItemsAndKeepsPositions() {
  const std::string json = R"json({"total":3,"next":"next-page","items":[
    {"item":{"uri":"spotify:track:t1","name":"First","duration_ms":1000,
      "artists":[{"name":"One"},{"name":"Two"}],"is_playable":true,
      "album":{"images":[{"url":"first.jpg","width":300}]}}},
    {"item":null},
    {"item":{"uri":"spotify:track:t3","name":"Third","duration_ms":3000,
      "artists":[{"name":"Three"}],"is_playable":false,"album":{"images":[]}}}
  ]})json";
  spotctl::SpotifyPage<spotctl::TrackSummary> page;
  EXPECT_TRUE(spotctl::parsePlaylistItems(json, 40, page));
  EXPECT_EQ(page.items.size(), static_cast<size_t>(1));
  EXPECT_EQ(page.items[0].artists, std::string("One, Two"));
  EXPECT_EQ(page.items[0].position, 40U);
  EXPECT_TRUE(page.has_more);
}

void deviceAndErrorParsersHandleSparseResponses() {
  const std::string devices_json = R"json({"devices":[
    {"id":"a","name":"Phone","type":"Smartphone","is_active":true,
      "is_restricted":false,"volume_percent":22},
    {"id":null,"name":"Web Player","type":"Computer","is_active":false,
      "is_restricted":true,"volume_percent":null}
  ]})json";
  std::vector<spotctl::PlaybackDevice> devices;
  EXPECT_TRUE(spotctl::parseDevices(devices_json, devices));
  EXPECT_EQ(devices.size(), static_cast<size_t>(2));
  EXPECT_EQ(devices[1].volume_percent, -1);

  const spotctl::SpotifyError error = spotctl::parseSpotifyError(
      429, R"json({"error":{"status":429,"message":"Slow down","reason":"QUOTA_EXCEEDED"}})json",
      7);
  EXPECT_EQ(error.category, ErrorCategory::RateLimited);
  EXPECT_EQ(error.retry_after_ms, 7000U);
  EXPECT_EQ(error.reason, std::string("QUOTA_EXCEEDED"));
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
  provisioningValidationMatchesTheDesktopUtility();
  playbackParserHandlesTracksAndEpisodes();
  playlistParserAppliesOwnershipRestriction();
  trackParserSkipsUnavailableItemsAndKeepsPositions();
  deviceAndErrorParsersHandleSparseResponses();

  if (failures != 0) {
    std::cerr << failures << " assertion(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "all core assertions passed\n";
  return EXIT_SUCCESS;
}
