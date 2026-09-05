#pragma once

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ArtworkFrame.h"

namespace spotctl {

class ArtworkManager {
public:
  ArtworkManager();

  // Full-size cover for the player screen.
  ArtworkHandle load(const std::string &url);
  // Row-sized cover, decoded straight to thumbnail scale so a list never
  // holds full resolution pixels it will not draw.
  ArtworkHandle loadThumbnail(const std::string &url);
  // Closes the kept-open connection. An idle TLS session holds ~50KB of
  // internal heap, which is heap the API's own handshakes need.
  void releaseConnection();

private:
  static bool jpegBlock(int16_t x, int16_t y, uint16_t width,
                        uint16_t height, uint16_t *bitmap);
  ArtworkHandle fetch(const std::string &url, uint16_t max_dimension,
                      size_t byte_limit);
  bool download(const std::string &url, size_t byte_limit, uint8_t *&data,
                size_t &length);
  ArtworkHandle decode(uint8_t *jpeg, size_t length, uint16_t max_dimension);
  void dropConnection();

  static ArtworkManager *decoding_instance_;
  NetworkClientSecure secure_client_;
  // Held open across covers: one TLS handshake serves a whole screenful.
  HTTPClient http_;
  std::string connected_host_;
  ArtworkFrame *decode_frame_{nullptr};
};

} // namespace spotctl
