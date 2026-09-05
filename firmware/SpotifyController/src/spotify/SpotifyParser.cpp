#include "SpotifyParser.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>

#include "../core/RuntimePolicy.h"

namespace spotctl {

namespace {

std::string text(JsonVariantConst value) {
  const char *result = value.as<const char *>();
  return result == nullptr ? std::string() : std::string(result);
}

std::string joinedArtists(JsonArrayConst artists) {
  std::string result;
  for (JsonObjectConst artist : artists) {
    const std::string name = text(artist["name"]);
    if (name.empty()) {
      continue;
    }
    if (!result.empty()) {
      result += ", ";
    }
    result += name;
  }
  return result;
}

std::string preferredImage(JsonArrayConst images) {
  std::string selected;
  int selected_distance = std::numeric_limits<int>::max();
  for (JsonObjectConst image : images) {
    const std::string url = text(image["url"]);
    if (url.empty()) {
      continue;
    }
    const int width = image["width"] | 0;
    const int distance = width > 0 ? std::abs(width - 300) : 10000;
    if (selected.empty() || distance < selected_distance) {
      selected = url;
      selected_distance = distance;
    }
  }
  return selected;
}

// Row thumbnails render near 40px, so the smallest variant Spotify offers is
// both sufficient and roughly an order of magnitude cheaper to download.
std::string smallestImage(JsonArrayConst images) {
  std::string selected;
  int selected_width = 0;
  for (JsonObjectConst image : images) {
    const std::string url = text(image["url"]);
    if (url.empty()) {
      continue;
    }
    const int width = image["width"] | 0;
    if (width > 0) {
      if (selected_width == 0 || width < selected_width) {
        selected = url;
        selected_width = width;
      }
    } else if (selected_width == 0) {
      // Widths are absent on mosaic covers, which arrive largest first.
      selected = url;
    }
  }
  return selected;
}

RepeatMode parseRepeat(const std::string &value) {
  if (value == "track") {
    return RepeatMode::Track;
  }
  if (value == "context") {
    return RepeatMode::Context;
  }
  return RepeatMode::Off;
}

bool parseTrackObject(JsonObjectConst track, uint32_t position,
                      TrackSummary &result) {
  if (track.isNull() || text(track["uri"]).empty()) {
    return false;
  }
  if (!track["is_playable"].isNull() && !track["is_playable"].as<bool>()) {
    return false;
  }
  result.uri = text(track["uri"]);
  result.title = text(track["name"]);
  result.artists = joinedArtists(track["artists"].as<JsonArrayConst>());
  result.duration_ms = track["duration_ms"] | 0U;
  result.artwork_url =
      preferredImage(track["album"]["images"].as<JsonArrayConst>());
  result.thumbnail_url =
      smallestImage(track["album"]["images"].as<JsonArrayConst>());
  result.position = position;
  return true;
}

bool parseTrackPage(const std::string &json, uint32_t page_offset,
                    bool saved_tracks, SpotifyPage<TrackSummary> &page) {
  JsonDocument document;
  if (deserializeJson(document, json)) {
    return false;
  }
  page.items.clear();
  page.total = document["total"] | 0U;
  page.has_more = !document["next"].isNull();
  uint32_t index = 0;
  for (JsonObjectConst wrapper : document["items"].as<JsonArrayConst>()) {
    JsonObjectConst item = saved_tracks
                               ? wrapper["track"].as<JsonObjectConst>()
                               : wrapper["item"].as<JsonObjectConst>();
    if (saved_tracks && item.isNull()) {
      item = wrapper["item"].as<JsonObjectConst>();
    }
    TrackSummary track;
    if (parseTrackObject(item, page_offset + index, track)) {
      page.items.push_back(std::move(track));
    }
    ++index;
  }
  return true;
}

} // namespace

bool parsePlayback(const std::string &json, uint32_t observed_at_ms,
                   PlaybackSnapshot &playback) {
  JsonDocument document;
  if (deserializeJson(document, json)) {
    return false;
  }

  PlaybackSnapshot parsed;
  parsed.observed_at_ms = observed_at_ms;
  parsed.is_playing = document["is_playing"] | false;
  parsed.progress_ms = document["progress_ms"] | 0U;
  parsed.shuffle = document["shuffle_state"] | false;
  parsed.repeat = parseRepeat(text(document["repeat_state"]));
  parsed.context_uri = text(document["context"]["uri"]);

  JsonObjectConst device = document["device"].as<JsonObjectConst>();
  parsed.device.id = text(device["id"]);
  parsed.device.name = text(device["name"]);
  parsed.device.type = text(device["type"]);
  parsed.device.active = device["is_active"] | false;
  parsed.device.restricted = device["is_restricted"] | false;
  parsed.device.volume_percent = device["volume_percent"].isNull()
                                     ? -1
                                     : device["volume_percent"].as<int>();
  parsed.volume_percent = parsed.device.volume_percent;

  JsonObjectConst item = document["item"].as<JsonObjectConst>();
  parsed.has_item = !item.isNull() && !text(item["uri"]).empty();
  if (parsed.has_item) {
    parsed.item.uri = text(item["uri"]);
    parsed.item.title = text(item["name"]);
    parsed.item.duration_ms = item["duration_ms"] | 0U;
    parsed.item.spotify_url = text(item["external_urls"]["spotify"]);
    const std::string type = text(document["currently_playing_type"]);
    if (type == "episode") {
      parsed.item.type = MediaType::Episode;
      parsed.item.subtitle = text(item["show"]["name"]);
      parsed.item.artwork_url =
          preferredImage(item["images"].as<JsonArrayConst>());
    } else {
      parsed.item.type = type == "track" ? MediaType::Track : MediaType::Unknown;
      parsed.item.subtitle = joinedArtists(item["artists"].as<JsonArrayConst>());
      parsed.item.artwork_url =
          preferredImage(item["album"]["images"].as<JsonArrayConst>());
    }
  }
  playback = std::move(parsed);
  return true;
}

bool parseDevices(const std::string &json,
                  std::vector<PlaybackDevice> &devices) {
  JsonDocument document;
  if (deserializeJson(document, json)) {
    return false;
  }
  devices.clear();
  for (JsonObjectConst item : document["devices"].as<JsonArrayConst>()) {
    PlaybackDevice device;
    device.id = text(item["id"]);
    device.name = text(item["name"]);
    device.type = text(item["type"]);
    device.active = item["is_active"] | false;
    device.restricted = item["is_restricted"] | false;
    device.volume_percent = item["volume_percent"].isNull()
                                ? -1
                                : item["volume_percent"].as<int>();
    devices.push_back(std::move(device));
  }
  return true;
}

bool parsePlaylists(const std::string &json,
                    const std::string &current_account_id,
                    SpotifyPage<PlaylistSummary> &page) {
  JsonDocument document;
  if (deserializeJson(document, json)) {
    return false;
  }
  page.items.clear();
  page.total = document["total"] | 0U;
  page.has_more = !document["next"].isNull();
  for (JsonObjectConst item : document["items"].as<JsonArrayConst>()) {
    PlaylistSummary playlist;
    playlist.id = text(item["id"]);
    playlist.uri = text(item["uri"]);
    playlist.name = text(item["name"]);
    playlist.owner = text(item["owner"]["display_name"]);
    playlist.artwork_url =
        preferredImage(item["images"].as<JsonArrayConst>());
    playlist.thumbnail_url =
        smallestImage(item["images"].as<JsonArrayConst>());
    playlist.collaborative = item["collaborative"] | false;
    std::string owner_id = text(item["owner"]["account_id"]);
    if (owner_id.empty()) {
      owner_id = text(item["owner"]["id"]);
    }
    playlist.owned = !current_account_id.empty() && owner_id == current_account_id;
    playlist.items_browsable =
        playlistItemsBrowsable(playlist.owned, playlist.collaborative);
    if (!playlist.id.empty()) {
      page.items.push_back(std::move(playlist));
    }
  }
  return true;
}

bool parsePlaylistItems(const std::string &json, uint32_t page_offset,
                        SpotifyPage<TrackSummary> &page) {
  return parseTrackPage(json, page_offset, false, page);
}

bool parseSavedTracks(const std::string &json, uint32_t page_offset,
                      SpotifyPage<TrackSummary> &page) {
  return parseTrackPage(json, page_offset, true, page);
}

SpotifyError parseSpotifyError(int status, const std::string &json,
                               uint32_t retry_after_seconds) {
  SpotifyError result;
  result.http_status = status;
  result.retry_after_ms = retry_after_seconds * 1000U;
  JsonDocument document;
  if (!json.empty() && !deserializeJson(document, json)) {
    result.reason = text(document["error"]["reason"]);
    result.user_message = text(document["error"]["message"]);
  }
  result.category = classifySpotifyError(status, result.reason);
  if (result.user_message.empty()) {
    // Nothing parseable came back, so the status is the only detail there is.
    // Without it a gateway rejection and a dead connection look identical.
    result.user_message =
        status > 0 ? "Spotify request failed (HTTP " + std::to_string(status) + ")"
                   : "Could not reach Spotify";
  }
  return result;
}

SpotifyError parseOAuthTokenError(int status, const std::string &json,
                                  uint32_t retry_after_seconds) {
  SpotifyError result;
  result.http_status = status;
  result.retry_after_ms = retry_after_seconds * 1000U;
  JsonDocument document;
  if (!json.empty() && !deserializeJson(document, json)) {
    result.reason = text(document["error"]);
    result.user_message = text(document["error_description"]);
  }
  if (result.reason == "invalid_grant" || result.reason == "invalid_client") {
    result.category = ErrorCategory::Authorization;
  } else if (status == 429) {
    result.category = ErrorCategory::RateLimited;
  } else if (status <= 0 || status >= 500 ||
             result.reason == "temporarily_unavailable") {
    result.category = ErrorCategory::Transient;
  } else {
    result.category = ErrorCategory::Permanent;
  }
  if (result.user_message.empty()) {
    result.user_message = result.category == ErrorCategory::Authorization
                              ? "Spotify login has expired"
                              : "Spotify authorization request failed";
  }
  return result;
}

} // namespace spotctl
