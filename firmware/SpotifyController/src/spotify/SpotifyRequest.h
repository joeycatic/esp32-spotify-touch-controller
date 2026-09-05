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

} // namespace spotctl

