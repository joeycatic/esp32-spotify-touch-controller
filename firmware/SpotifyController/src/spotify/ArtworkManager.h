#pragma once

#include <NetworkClientSecure.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ArtworkFrame.h"

namespace spotctl {

class ArtworkManager {
public:
  ArtworkManager();
  ArtworkHandle load(const std::string &url);

private:
  static bool jpegBlock(int16_t x, int16_t y, uint16_t width,
                        uint16_t height, uint16_t *bitmap);
  bool download(const std::string &url, uint8_t *&data, size_t &length);
  ArtworkHandle decode(uint8_t *jpeg, size_t length);

  static ArtworkManager *decoding_instance_;
  NetworkClientSecure secure_client_;
  ArtworkFrame *decode_frame_{nullptr};
};

} // namespace spotctl
