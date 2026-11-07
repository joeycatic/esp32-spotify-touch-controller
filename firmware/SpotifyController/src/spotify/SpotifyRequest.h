#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spotctl {

// True when HTTPClient would otherwise frame the request with no length at
// all: it only emits Content-Length for a non-empty payload, and Spotify's
// edge rejects a bodyless PUT or POST with 411 Length Required.
bool requiresZeroContentLength(const char *method, const std::string &body);
// Host portion of an absolute URL, empty when there is none. Keep-alive reuse
// is only safe within one host, and HTTPClient itself does not check.
std::string hostOf(const std::string &url);
std::string urlEncode(const std::string &value);
std::string playContextBody(const std::string &context_uri,
                            uint32_t position);
std::string playUrisBody(const std::vector<std::string> &uris);
// Enqueue passes the track URI in the query string, where its colons must be
// percent-encoded.
std::string queueUriPath(const std::string &uri);
// The URI itself when it names something /me/library accepts, empty otherwise.
// Local files have no library identity and are rejected.
std::string librarySavableUri(const std::string &uri);
// February 2026 replaced the entity-specific saved-items endpoints with these
// two, which address items by URI in the query string.
std::string libraryContainsPath(const std::string &uri);
std::string libraryItemsPath(const std::string &uri);

} // namespace spotctl

