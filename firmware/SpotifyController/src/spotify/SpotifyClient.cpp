#include "SpotifyClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include <algorithm>

#include "SpotifyRequest.h"

extern const uint8_t spotify_crt_bundle_start[]
    asm("_binary_x509_crt_bundle_start");
extern const uint8_t spotify_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

namespace spotctl {

namespace {
constexpr char kApiBase[] = "https://api.spotify.com/v1";
constexpr char kTokenUrl[] = "https://accounts.spotify.com/api/token";

bool successful(int status) { return status >= 200 && status < 300; }

std::string jsonText(JsonVariantConst value) {
  const char *text = value.as<const char *>();
  return text == nullptr ? std::string() : std::string(text);
}
} // namespace

SpotifyClient::SpotifyClient(ConfigStore &store) : store_(store) {
  secure_client_.setCACertBundle(
      spotify_crt_bundle_start,
      static_cast<size_t>(spotify_crt_bundle_end - spotify_crt_bundle_start));
  secure_client_.setHandshakeTimeout(15);
}

void SpotifyClient::begin(const DeviceConfig &config) {
  client_id_ = config.client_id;
  refresh_token_ = config.refresh_token;
  access_token_.clear();
  account_id_.clear();
  refresh_at_ms_ = 0;
}

SpotifyClient::HttpResponse
SpotifyClient::requestRaw(const char *method, const std::string &url,
                          const std::string &body, const char *content_type,
                          bool authenticated) {
  HttpResponse result;
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setReuse(false);
  const char *headers[] = {"Retry-After"};
  http.collectHeaders(headers, 1);
  if (!http.begin(secure_client_, url.c_str())) {
    result.status = 0;
    return result;
  }
  if (authenticated) {
    const String authorization = String("Bearer ") + access_token_.c_str();
    http.addHeader("Authorization", authorization);
  }
  if (content_type != nullptr) {
    http.addHeader("Content-Type", content_type);
  }
  const uint8_t *bytes = body.empty()
                             ? nullptr
                             : reinterpret_cast<const uint8_t *>(body.data());
  result.status = http.sendRequest(method, const_cast<uint8_t *>(bytes),
                                   body.size());
  if (result.status > 0) {
    result.body = http.getString().c_str();
    const String retry_after = http.header("Retry-After");
    if (!retry_after.isEmpty()) {
      result.retry_after_seconds = static_cast<uint32_t>(retry_after.toInt());
    }
  }
  http.end();
  return result;
}

bool SpotifyClient::refreshAccessToken(SpotifyError &error) {
  const std::string body =
      "client_id=" + urlEncode(client_id_) + "&grant_type=refresh_token&refresh_token=" +
      urlEncode(refresh_token_);
  const HttpResponse response = requestRaw(
      "POST", kTokenUrl, body, "application/x-www-form-urlencoded", false);
  if (!successful(response.status)) {
    error = parseOAuthTokenError(response.status, response.body,
                                 response.retry_after_seconds);
    return false;
  }
  JsonDocument document;
  if (deserializeJson(document, response.body)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify returned an invalid token response";
    return false;
  }
  access_token_ = jsonText(document["access_token"]);
  if (access_token_.empty()) {
    error = parseSpotifyError(401, "");
    error.user_message = "Spotify did not return an access token";
    return false;
  }
  const uint32_t expires_seconds = document["expires_in"] | 3600U;
  const uint32_t usable_seconds = expires_seconds > 60 ? expires_seconds - 60 : 30;
  refresh_at_ms_ = millis() + usable_seconds * 1000U;

  const std::string rotated_refresh = jsonText(document["refresh_token"]);
  if (!rotated_refresh.empty() && rotated_refresh != refresh_token_) {
    if (!store_.updateRefreshToken(rotated_refresh)) {
      error = parseSpotifyError(0, "");
      error.user_message = "Could not save Spotify's refreshed login";
      return false;
    }
    refresh_token_ = rotated_refresh;
  }
  return true;
}

bool SpotifyClient::ensureAccessToken(SpotifyError &error) {
  const bool expired = access_token_.empty() ||
                       static_cast<int32_t>(millis() - refresh_at_ms_) >= 0;
  return !expired || refreshAccessToken(error);
}

bool SpotifyClient::apiRequest(const char *method, const std::string &path,
                               const std::string &body, HttpResponse &response,
                               SpotifyError &error) {
  if (!ensureAccessToken(error)) {
    return false;
  }
  response = requestRaw(method, std::string(kApiBase) + path, body,
                        body.empty() ? nullptr : "application/json", true);
  if (response.status == 401 && refreshAccessToken(error)) {
    response = requestRaw(method, std::string(kApiBase) + path, body,
                          body.empty() ? nullptr : "application/json", true);
  }
  if (!successful(response.status)) {
    error = parseSpotifyError(response.status, response.body,
                              response.retry_after_seconds);
    return false;
  }
  return true;
}

bool SpotifyClient::command(const char *method, const std::string &path,
                            const std::string &body, SpotifyError &error) {
  HttpResponse response;
  return apiRequest(method, path, body, response, error);
}

bool SpotifyClient::loadAccountId(std::string &account_id,
                                  SpotifyError &error) {
  HttpResponse response;
  if (!apiRequest("GET", "/me", {}, response, error)) {
    return false;
  }
  JsonDocument document;
  if (deserializeJson(document, response.body)) {
    error = parseSpotifyError(0, "");
    return false;
  }
  account_id_ = jsonText(document["account_id"]);
  if (account_id_.empty()) {
    account_id_ = jsonText(document["id"]);
  }
  account_id = account_id_;
  if (account_id_.empty()) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify profile did not contain an account identifier";
    return false;
  }
  return true;
}

bool SpotifyClient::getPlayback(PlaybackSnapshot &playback,
                                SpotifyError &error) {
  HttpResponse response;
  if (!apiRequest("GET", "/me/player", {}, response, error)) {
    return false;
  }
  if (response.status == 204 || response.body.empty()) {
    playback = PlaybackSnapshot{};
    playback.observed_at_ms = millis();
    return true;
  }
  if (!parsePlayback(response.body, millis(), playback)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify playback response could not be read";
    return false;
  }
  return true;
}

bool SpotifyClient::getDevices(std::vector<PlaybackDevice> &devices,
                               SpotifyError &error) {
  HttpResponse response;
  if (!apiRequest("GET", "/me/player/devices", {}, response, error)) {
    return false;
  }
  if (!parseDevices(response.body, devices)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify device response could not be read";
    return false;
  }
  return true;
}

bool SpotifyClient::getPlaylists(uint32_t offset,
                                 SpotifyPage<PlaylistSummary> &page,
                                 SpotifyError &error) {
  HttpResponse response;
  const std::string path = "/me/playlists?limit=20&offset=" +
                           std::to_string(offset);
  if (!apiRequest("GET", path, {}, response, error)) {
    return false;
  }
  if (!parsePlaylists(response.body, account_id_, page)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify playlist response could not be read";
    return false;
  }
  return true;
}

bool SpotifyClient::getPlaylistItems(const std::string &playlist_id,
                                     uint32_t offset,
                                     SpotifyPage<TrackSummary> &page,
                                     SpotifyError &error) {
  HttpResponse response;
  const std::string path = "/playlists/" + urlEncode(playlist_id) +
                           "/items?limit=20&offset=" + std::to_string(offset);
  if (!apiRequest("GET", path, {}, response, error)) {
    return false;
  }
  if (!parsePlaylistItems(response.body, offset, page)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify song response could not be read";
    return false;
  }
  return true;
}

bool SpotifyClient::getSavedTracks(uint32_t offset,
                                   SpotifyPage<TrackSummary> &page,
                                   SpotifyError &error) {
  HttpResponse response;
  const std::string path =
      "/me/tracks?limit=20&offset=" + std::to_string(offset);
  if (!apiRequest("GET", path, {}, response, error)) {
    return false;
  }
  if (!parseSavedTracks(response.body, offset, page)) {
    error = parseSpotifyError(0, "");
    error.user_message = "Spotify Liked Songs response could not be read";
    return false;
  }
  return true;
}

bool SpotifyClient::resume(SpotifyError &error) {
  return command("PUT", "/me/player/play", {}, error);
}

bool SpotifyClient::pause(SpotifyError &error) {
  return command("PUT", "/me/player/pause", {}, error);
}

bool SpotifyClient::next(SpotifyError &error) {
  return command("POST", "/me/player/next", {}, error);
}

bool SpotifyClient::previous(SpotifyError &error) {
  return command("POST", "/me/player/previous", {}, error);
}

bool SpotifyClient::seek(uint32_t position_ms, SpotifyError &error) {
  return command("PUT", "/me/player/seek?position_ms=" +
                            std::to_string(position_ms),
                 {}, error);
}

bool SpotifyClient::setVolume(uint8_t percent, SpotifyError &error) {
  const uint8_t bounded = std::min<uint8_t>(percent, 100);
  return command("PUT", "/me/player/volume?volume_percent=" +
                            std::to_string(bounded),
                 {}, error);
}

bool SpotifyClient::setShuffle(bool enabled, SpotifyError &error) {
  return command("PUT", std::string("/me/player/shuffle?state=") +
                            (enabled ? "true" : "false"),
                 {}, error);
}

bool SpotifyClient::setRepeat(RepeatMode mode, SpotifyError &error) {
  const char *state = mode == RepeatMode::Track
                          ? "track"
                          : (mode == RepeatMode::Context ? "context" : "off");
  return command("PUT", std::string("/me/player/repeat?state=") + state, {},
                 error);
}

bool SpotifyClient::transferPlayback(const std::string &device_id,
                                     SpotifyError &error) {
  JsonDocument document;
  document["device_ids"].to<JsonArray>().add(device_id);
  document["play"] = false;
  std::string body;
  serializeJson(document, body);
  return command("PUT", "/me/player", body, error);
}

bool SpotifyClient::playContext(const std::string &context_uri,
                                uint32_t position, SpotifyError &error) {
  return command("PUT", "/me/player/play",
                 playContextBody(context_uri, position), error);
}

bool SpotifyClient::playUris(const std::vector<std::string> &uris,
                             SpotifyError &error) {
  if (uris.empty()) {
    error = parseSpotifyError(400, "");
    error.user_message = "No playable songs were selected";
    return false;
  }
  return command("PUT", "/me/player/play", playUrisBody(uris), error);
}

} // namespace spotctl
