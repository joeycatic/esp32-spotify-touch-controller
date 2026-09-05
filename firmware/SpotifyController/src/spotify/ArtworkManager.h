#pragma once

#include <NetworkClientSecure.h>
#include <lvgl.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace spotctl {

class ArtworkManager {
public:
  ArtworkManager();
  const lv_img_dsc_t *load(const std::string &url);

private:
  struct Slot {
    uint16_t *pixels{nullptr};
    lv_img_dsc_t image{};
    uint16_t width{0};
    uint16_t height{0};
  };

  static bool jpegBlock(int16_t x, int16_t y, uint16_t width,
                        uint16_t height, uint16_t *bitmap);
  bool download(const std::string &url, uint8_t *&data, size_t &length);
  bool decode(uint8_t *jpeg, size_t length, Slot &slot);

  static ArtworkManager *decoding_instance_;
  NetworkClientSecure secure_client_;
  Slot slots_[2];
  uint8_t active_slot_{0};
  Slot *decode_slot_{nullptr};
};

} // namespace spotctl

