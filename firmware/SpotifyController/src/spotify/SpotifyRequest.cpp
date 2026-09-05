#include "SpotifyRequest.h"

#include <ArduinoJson.h>

namespace spotctl {

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

