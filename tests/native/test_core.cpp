#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/AppState.h"
#include "core/RuntimePolicy.h"
#include "provision/ProvisioningValidation.h"
#include "spotify/SpotifyParser.h"
#include "spotify/SpotifyRequest.h"
#include "ui/AnimationPolicy.h"
#include "ui/EventBinding.h"

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

void oauthTokenErrorsRequireReauthorization() {
  const spotctl::SpotifyError invalid_grant = spotctl::parseOAuthTokenError(
      400,
      R"json({"error":"invalid_grant","error_description":"Refresh token revoked"})json");
  EXPECT_EQ(invalid_grant.category, ErrorCategory::Authorization);
  EXPECT_EQ(invalid_grant.reason, std::string("invalid_grant"));
  EXPECT_EQ(invalid_grant.user_message, std::string("Refresh token revoked"));

  const spotctl::SpotifyError temporary = spotctl::parseOAuthTokenError(
      503, R"json({"error":"temporarily_unavailable"})json");
  EXPECT_EQ(temporary.category, ErrorCategory::Transient);
}

void offlineCommandsAreRejectedAndOptimisticStateIsCoherent() {
  EXPECT_FALSE(spotctl::commandAccepted(true, false, false));
  EXPECT_FALSE(spotctl::commandAccepted(false, true, false));
  EXPECT_FALSE(spotctl::commandAccepted(true, true, true));
  EXPECT_TRUE(spotctl::commandAccepted(true, true, false));

  PlaybackSnapshot playback;
  playback.is_playing = true;
  playback.shuffle = false;
  playback.repeat = spotctl::RepeatMode::Off;
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::TogglePlaying);
  EXPECT_FALSE(playback.is_playing);
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::TogglePlaying);
  EXPECT_TRUE(playback.is_playing);
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::ToggleShuffle);
  EXPECT_TRUE(playback.shuffle);
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::CycleRepeat);
  EXPECT_EQ(playback.repeat, spotctl::RepeatMode::Context);
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::CycleRepeat);
  EXPECT_EQ(playback.repeat, spotctl::RepeatMode::Track);
  spotctl::applyOptimisticPlayback(playback,
                                   spotctl::PlaybackMutation::CycleRepeat);
  EXPECT_EQ(playback.repeat, spotctl::RepeatMode::Off);
}

void spotifyApiWorkPreemptsThumbnailConnections() {
  EXPECT_TRUE(spotctl::networkWorkNeedsArtworkRelease(
      spotctl::NetworkWork::SpotifyApi));
  EXPECT_TRUE(spotctl::networkWorkNeedsArtworkRelease(
      spotctl::NetworkWork::PlaybackPoll));
  EXPECT_FALSE(spotctl::networkWorkNeedsArtworkRelease(
      spotctl::NetworkWork::Thumbnail));
}

void playlistPrefetchIsReusedAndFailedLoadsCanRetry() {
  spotctl::PlaylistLoadState state;

  EXPECT_TRUE(state.shouldRequest(0));
  state.markRequested();
  EXPECT_FALSE(state.shouldRequest(0));

  state.markLoaded();
  EXPECT_FALSE(state.shouldRequest(20));

  spotctl::PlaylistLoadState failed;
  failed.markRequested();
  failed.markFailed();
  EXPECT_TRUE(failed.shouldRequest(0));
}

void rebuildingAViewKeepsOneGestureHandler() {
  size_t registered_handlers = 0;
  const auto rebuild_view = [&registered_handlers]() {
    spotctl::bindSingleEventHandler(
        [&registered_handlers]() {
          if (registered_handlers == 0) {
            return false;
          }
          --registered_handlers;
          return true;
        },
        [&registered_handlers]() { ++registered_handlers; });
  };

  rebuild_view();
  EXPECT_EQ(registered_handlers, static_cast<size_t>(1));

  rebuild_view();
  EXPECT_EQ(registered_handlers, static_cast<size_t>(1));
}

void swipeFeedbackMovesWithPlaybackDirectionAndStaysShort() {
  const spotctl::SwipeAnimationPlan next =
      spotctl::swipeAnimationPlan(spotctl::SwipeDirection::Next);
  const spotctl::SwipeAnimationPlan previous =
      spotctl::swipeAnimationPlan(spotctl::SwipeDirection::Previous);

  EXPECT_TRUE(next.offset_px < 0);
  EXPECT_TRUE(previous.offset_px > 0);
  EXPECT_EQ(std::abs(next.offset_px), std::abs(previous.offset_px));
  EXPECT_TRUE(next.outward_ms > 0);
  EXPECT_TRUE(next.return_ms > 0);
  EXPECT_TRUE(next.outward_ms + next.return_ms <= 250);
  EXPECT_EQ(next.outward_ms, previous.outward_ms);
  EXPECT_EQ(next.return_ms, previous.return_ms);
}

void buttonFeedbackIsSubtleAndBrief() {
  const spotctl::ButtonAnimationPlan button = spotctl::buttonAnimationPlan();

  EXPECT_TRUE(button.pressed_translate_y_px > 0);
  EXPECT_TRUE(button.pressed_translate_y_px <= 2);
  EXPECT_TRUE(button.press_ms > 0);
  EXPECT_TRUE(button.release_ms >= button.press_ms);
  EXPECT_TRUE(button.press_ms + button.release_ms <= 200);
}

void provisioningValidationMatchesTheDesktopUtility() {
  spotctl::ProvisioningFields valid{
      "Studio WiFi", "correct horse battery staple",
      "0123456789abcdef0123456789abcdef", "AQD-refresh-token"};
  EXPECT_EQ(spotctl::validateProvisioning(valid), std::string());

  valid.ssid = std::string(33, 'x');
  EXPECT_EQ(spotctl::validateProvisioning(valid),
            std::string("invalid_wifi_ssid"));

  std::string seventeen_utf8_characters;
  for (int index = 0; index < 17; ++index) {
    seventeen_utf8_characters += "\xC3\xA4";
  }
  valid.ssid = seventeen_utf8_characters;
  EXPECT_EQ(valid.ssid.size(), static_cast<size_t>(34));
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

void spotifyRequestEncodingProtectsQueryAndJsonBoundaries() {
  EXPECT_EQ(spotctl::urlEncode("client id+/="),
            std::string("client%20id%2B%2F%3D"));
  EXPECT_EQ(spotctl::playContextBody("spotify:playlist:abc", 17),
            std::string("{\"context_uri\":\"spotify:playlist:abc\",\"offset\":{\"position\":17}}"));
  EXPECT_EQ(spotctl::playUrisBody({"spotify:track:a", "spotify:track:b"}),
            std::string("{\"uris\":[\"spotify:track:a\",\"spotify:track:b\"]}"));
}

void bodylessWriteRequestsMustDeclareZeroContentLength() {
  // Spotify's edge answers 411 Length Required when a PUT or POST arrives with
  // neither Content-Length nor Transfer-Encoding, and the HTML error body then
  // parses as no Spotify error at all.
  EXPECT_TRUE(spotctl::requiresZeroContentLength("PUT", ""));
  EXPECT_TRUE(spotctl::requiresZeroContentLength("POST", ""));
  EXPECT_TRUE(spotctl::requiresZeroContentLength("DELETE", ""));

  // A body of its own already makes HTTPClient emit Content-Length.
  EXPECT_FALSE(spotctl::requiresZeroContentLength("PUT", "{\"play\":false}"));

  // Reads carry no entity, so no length header belongs on them.
  EXPECT_FALSE(spotctl::requiresZeroContentLength("GET", ""));
  EXPECT_FALSE(spotctl::requiresZeroContentLength("HEAD", ""));
}

void parsersPickTheSmallestCoverForRowThumbnails() {
  // Spotify orders images largest first; a row thumbnail is ~40px, so the
  // smallest variant is both enough and far cheaper to fetch.
  const std::string json = R"json({"total":1,"next":null,"items":[
    {"id":"mine","uri":"spotify:playlist:mine","name":"My Mix","collaborative":false,
      "owner":{"account_id":"account-1","display_name":"Joey"},
      "images":[{"url":"big.jpg","width":640},{"url":"mid.jpg","width":300},
                {"url":"small.jpg","width":60}]}
  ]})json";
  spotctl::SpotifyPage<spotctl::PlaylistSummary> page;
  EXPECT_TRUE(spotctl::parsePlaylists(json, "account-1", page));
  EXPECT_EQ(page.items[0].artwork_url, std::string("mid.jpg"));
  EXPECT_EQ(page.items[0].thumbnail_url, std::string("small.jpg"));

  // Mosaic covers arrive without width fields, ordered largest first.
  const std::string mosaic = R"json({"total":1,"next":null,"items":[
    {"id":"m","uri":"spotify:playlist:m","name":"Mosaic","collaborative":false,
      "owner":{"account_id":"account-1","display_name":"Joey"},
      "images":[{"url":"m640.jpg"},{"url":"m60.jpg"}]}
  ]})json";
  spotctl::SpotifyPage<spotctl::PlaylistSummary> mosaic_page;
  EXPECT_TRUE(spotctl::parsePlaylists(mosaic, "account-1", mosaic_page));
  EXPECT_EQ(mosaic_page.items[0].thumbnail_url, std::string("m60.jpg"));

  // A coverless playlist leaves the row on its symbol.
  const std::string bare = R"json({"total":1,"next":null,"items":[
    {"id":"b","uri":"spotify:playlist:b","name":"Bare","collaborative":false,
      "owner":{"account_id":"account-1","display_name":"Joey"},"images":[]}
  ]})json";
  spotctl::SpotifyPage<spotctl::PlaylistSummary> bare_page;
  EXPECT_TRUE(spotctl::parsePlaylists(bare, "account-1", bare_page));
  EXPECT_TRUE(bare_page.items[0].thumbnail_url.empty());
}

void trackRowsCarryTheirOwnThumbnail() {
  const std::string json = R"json({"total":1,"next":null,"items":[
    {"item":{"uri":"spotify:track:1","name":"Song","duration_ms":1000,
      "is_playable":true,"artists":[{"name":"Band"}],
      "album":{"images":[{"url":"a640.jpg","width":640},{"url":"a300.jpg","width":300},
                         {"url":"a64.jpg","width":64}]}}}
  ]})json";
  spotctl::SpotifyPage<spotctl::TrackSummary> page;
  EXPECT_TRUE(spotctl::parsePlaylistItems(json, 0, page));
  EXPECT_EQ(page.items.size(), static_cast<size_t>(1));
  if (page.items.size() == 1) {
    EXPECT_EQ(page.items[0].artwork_url, std::string("a300.jpg"));
    EXPECT_EQ(page.items[0].thumbnail_url, std::string("a64.jpg"));
  }
}

void hostExtractionGuardsConnectionReuse() {
  // Reusing a keep-alive socket across hosts would send a request down the
  // wrong TLS connection, so the fetcher compares hosts before reusing.
  EXPECT_EQ(spotctl::hostOf("https://i.scdn.co/image/ab12"),
            std::string("i.scdn.co"));
  EXPECT_EQ(spotctl::hostOf("https://mosaic.scdn.co/640/abc"),
            std::string("mosaic.scdn.co"));
  EXPECT_EQ(spotctl::hostOf("https://i.scdn.co"), std::string("i.scdn.co"));
  EXPECT_EQ(spotctl::hostOf("https://i.scdn.co:8443/x"),
            std::string("i.scdn.co"));
  EXPECT_EQ(spotctl::hostOf("not a url"), std::string());
  EXPECT_EQ(spotctl::hostOf(""), std::string());
}

void onlyRowsTouchingTheViewportAreFetched() {
  // Viewport spanning display rows 44..266.
  EXPECT_TRUE(spotctl::rowIntersectsViewport(100, 152, 44, 266));

  // Straddling either edge still counts: the row is partly visible.
  EXPECT_TRUE(spotctl::rowIntersectsViewport(20, 60, 44, 266));
  EXPECT_TRUE(spotctl::rowIntersectsViewport(250, 302, 44, 266));

  // Scrolled well past, in either direction, is skipped.
  EXPECT_FALSE(spotctl::rowIntersectsViewport(-60, -8, 44, 266));
  EXPECT_FALSE(spotctl::rowIntersectsViewport(300, 352, 44, 266));

  // Touching exactly at an edge is visible; one pixel beyond is not.
  EXPECT_TRUE(spotctl::rowIntersectsViewport(-8, 44, 44, 266));
  EXPECT_FALSE(spotctl::rowIntersectsViewport(-8, 43, 44, 266));

  // Inverted or unmeasured geometry asks for nothing.
  EXPECT_FALSE(spotctl::rowIntersectsViewport(100, 40, 44, 266));
  EXPECT_FALSE(spotctl::rowIntersectsViewport(100, 152, 266, 44));
}

void thumbnailCacheEvictsLeastRecentlyUsed() {
  spotctl::LruCache<std::shared_ptr<int>> cache(3);
  cache.put("a", std::make_shared<int>(1));
  cache.put("b", std::make_shared<int>(2));
  cache.put("c", std::make_shared<int>(3));

  // Touching "a" makes "b" the least recently used.
  EXPECT_TRUE(cache.get("a") != nullptr);
  cache.put("d", std::make_shared<int>(4));
  EXPECT_EQ(cache.size(), static_cast<size_t>(3));
  EXPECT_TRUE(cache.get("b") == nullptr);
  EXPECT_TRUE(cache.get("a") != nullptr);
  EXPECT_TRUE(cache.get("d") != nullptr);

  // A remembered failure is a stored empty value, not a miss, so a broken
  // cover is not re-fetched on every scroll.
  cache.put("bad", std::shared_ptr<int>());
  EXPECT_TRUE(cache.contains("bad"));
  EXPECT_TRUE(*cache.get("bad") == nullptr);
}

void thumbnailQueueDropsDuplicatesAndStaleWindows() {
  spotctl::ThumbnailQueue queue(4);
  EXPECT_TRUE(queue.push("k1", "u1"));
  EXPECT_TRUE(queue.push("k2", "u2"));
  EXPECT_FALSE(queue.push("k1", "u1"));
  EXPECT_EQ(queue.size(), static_cast<size_t>(2));

  std::string key;
  std::string url;
  EXPECT_TRUE(queue.pop(key, url));
  EXPECT_EQ(key, std::string("k1"));
  EXPECT_EQ(url, std::string("u1"));

  // Scrolling away abandons rows that never got fetched.
  queue.clear();
  EXPECT_EQ(queue.size(), static_cast<size_t>(0));
  EXPECT_FALSE(queue.pop(key, url));

  // The queue is bounded so a long list cannot grow it without limit.
  EXPECT_TRUE(queue.push("a", "ua"));
  EXPECT_TRUE(queue.push("b", "ub"));
  EXPECT_TRUE(queue.push("c", "uc"));
  EXPECT_TRUE(queue.push("d", "ud"));
  EXPECT_FALSE(queue.push("e", "ue"));
  EXPECT_EQ(queue.size(), static_cast<size_t>(4));
}

void unexplainedFailuresNameTheStatusTheyGotBack() {
  // Spotify's own message wins whenever the body carries one.
  EXPECT_EQ(spotctl::parseSpotifyError(
                404, R"json({"error":{"status":404,"message":"No active device"}})json")
                .user_message,
            std::string("No active device"));

  // A gateway error body is HTML, not Spotify JSON, so the status is all the
  // detail there is - and it is what makes the failure diagnosable.
  EXPECT_EQ(spotctl::parseSpotifyError(411, "<html>Length Required</html>")
                .user_message,
            std::string("Spotify request failed (HTTP 411)"));
  EXPECT_EQ(spotctl::parseSpotifyError(502, "").user_message,
            std::string("Spotify request failed (HTTP 502)"));

  // A negative status is a connection that never produced a response at all.
  EXPECT_EQ(spotctl::parseSpotifyError(-1, "").user_message,
            std::string("Could not reach Spotify"));
  EXPECT_EQ(spotctl::parseSpotifyError(0, "").user_message,
            std::string("Could not reach Spotify"));
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
  oauthTokenErrorsRequireReauthorization();
  offlineCommandsAreRejectedAndOptimisticStateIsCoherent();
  spotifyApiWorkPreemptsThumbnailConnections();
  playlistPrefetchIsReusedAndFailedLoadsCanRetry();
  rebuildingAViewKeepsOneGestureHandler();
  swipeFeedbackMovesWithPlaybackDirectionAndStaysShort();
  buttonFeedbackIsSubtleAndBrief();
  provisioningValidationMatchesTheDesktopUtility();
  playbackParserHandlesTracksAndEpisodes();
  playlistParserAppliesOwnershipRestriction();
  trackParserSkipsUnavailableItemsAndKeepsPositions();
  deviceAndErrorParsersHandleSparseResponses();
  spotifyRequestEncodingProtectsQueryAndJsonBoundaries();
  bodylessWriteRequestsMustDeclareZeroContentLength();
  parsersPickTheSmallestCoverForRowThumbnails();
  trackRowsCarryTheirOwnThumbnail();
  hostExtractionGuardsConnectionReuse();
  onlyRowsTouchingTheViewportAreFetched();
  unexplainedFailuresNameTheStatusTheyGotBack();
  thumbnailCacheEvictsLeastRecentlyUsed();
  thumbnailQueueDropsDuplicatesAndStaleWindows();

  if (failures != 0) {
    std::cerr << failures << " assertion(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "all core assertions passed\n";
  return EXIT_SUCCESS;
}
