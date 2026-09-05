#include "SpotifyRequest.h"

#include <ArduinoJson.h>

#include <cstring>

namespace spotctl {

bool requiresZeroContentLength(const char *method, const std::string &body) {
  if (method == nullptr || !body.empty()) {
    return false;
  }
  return std::strcmp(method, "GET") != 0 && std::strcmp(method, "HEAD") != 0;
}

std::string hostOf(const std::string &url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) {
    return {};
  }
  const size_t start = scheme + 3;
  const size_t end = url.find_first_of("/:?#", start);
  if (end == std::string::npos) {
    return url.substr(start);
  }
  return url.substr(start, end - start);
}

std::string urlEncode(const std::string &value) {
  constexpr char hexadecimal[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (const unsigned char character : value) {
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.' || character == '~') {
      encoded.push_back(static_cast<char>(character));
    } else {
      encoded.push_back('%');
      encoded.push_back(hexadecimal[(character >> 4) & 0x0F]);
      encoded.push_back(hexadecimal[character & 0x0F]);
    }
  }
  return encoded;
}

std::string playContextBody(const std::string &context_uri,
                            uint32_t position) {
  JsonDocument document;
  document["context_uri"] = context_uri;
  document["offset"]["position"] = position;
  std::string result;
  serializeJson(document, result);
  return result;
}

std::string playUrisBody(const std::vector<std::string> &uris) {
  JsonDocument document;
  JsonArray output = document["uris"].to<JsonArray>();
  for (const auto &uri : uris) {
    output.add(uri);
  }
  std::string result;
  serializeJson(document, result);
  return result;
}

} // namespace spotctl

