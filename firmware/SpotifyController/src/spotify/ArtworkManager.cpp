#include "ArtworkManager.h"

#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

extern const uint8_t spotify_art_crt_bundle_start[]
    asm("_binary_x509_crt_bundle_start");
extern const uint8_t spotify_art_crt_bundle_end[]
    asm("_binary_x509_crt_bundle_end");

namespace spotctl {

namespace {
constexpr size_t kMaximumJpegBytes = 512U * 1024U;
constexpr uint16_t kMaximumDimension = 640;
} // namespace

ArtworkManager *ArtworkManager::decoding_instance_ = nullptr;

ArtworkManager::ArtworkManager() {
  secure_client_.setCACertBundle(
      spotify_art_crt_bundle_start,
      static_cast<size_t>(spotify_art_crt_bundle_end -
                          spotify_art_crt_bundle_start));
  secure_client_.setHandshakeTimeout(15);
  TJpgDec.setCallback(jpegBlock);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
}

bool ArtworkManager::download(const std::string &url, uint8_t *&data,
                              size_t &length) {
  data = nullptr;
  length = 0;
  if (url.empty()) {
    return false;
  }
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(secure_client_, url.c_str())) {
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int announced_length = http.getSize();
  if (announced_length > static_cast<int>(kMaximumJpegBytes)) {
    http.end();
    return false;
  }
  const size_t capacity = announced_length > 0
                              ? static_cast<size_t>(announced_length)
                              : kMaximumJpegBytes;
  data = static_cast<uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM));
  if (data == nullptr) {
    http.end();
    return false;
  }

  NetworkClient *stream = http.getStreamPtr();
  const uint32_t started = millis();
  while (http.connected() && length < capacity && millis() - started < 15000) {
    const size_t available = stream->available();
    if (available == 0) {
      delay(2);
      continue;
    }
    const size_t chunk = std::min(available, capacity - length);
    const size_t received = stream->readBytes(data + length, chunk);
    if (received == 0) {
      break;
    }
    length += received;
    if (announced_length > 0 && length == static_cast<size_t>(announced_length)) {
      break;
    }
  }
  http.end();
  if (length == 0 ||
      (announced_length > 0 && length != static_cast<size_t>(announced_length))) {
    heap_caps_free(data);
    data = nullptr;
    length = 0;
    return false;
  }
  return true;
}

bool ArtworkManager::decode(uint8_t *jpeg, size_t length, Slot &slot) {
  uint16_t width = 0;
  uint16_t height = 0;
  if (TJpgDec.getJpgSize(&width, &height, jpeg, length) != JDR_OK || width == 0 ||
      height == 0 || width > kMaximumDimension || height > kMaximumDimension) {
    return false;
  }
  uint16_t *new_pixels = static_cast<uint16_t *>(heap_caps_malloc(
      static_cast<size_t>(width) * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  if (new_pixels == nullptr) {
    return false;
  }
  if (slot.pixels != nullptr) {
    heap_caps_free(slot.pixels);
  }
  slot.pixels = new_pixels;
  slot.width = width;
  slot.height = height;
  decode_slot_ = &slot;
  decoding_instance_ = this;
  const JRESULT result = TJpgDec.drawJpg(0, 0, jpeg, length);
  decoding_instance_ = nullptr;
  decode_slot_ = nullptr;
  if (result != JDR_OK) {
    heap_caps_free(slot.pixels);
    slot.pixels = nullptr;
    return false;
  }

  slot.image.header.always_zero = 0;
  slot.image.header.cf = LV_IMG_CF_TRUE_COLOR;
  slot.image.header.w = width;
  slot.image.header.h = height;
  slot.image.data_size = static_cast<uint32_t>(width) * height * sizeof(uint16_t);
  slot.image.data = reinterpret_cast<const uint8_t *>(slot.pixels);
  return true;
}

bool ArtworkManager::jpegBlock(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t *bitmap) {
  ArtworkManager *manager = decoding_instance_;
  if (manager == nullptr || manager->decode_slot_ == nullptr) {
    return false;
  }
  Slot &slot = *manager->decode_slot_;
  if (x < 0 || y < 0 || x + width > slot.width || y + height > slot.height) {
    return false;
  }
  for (uint16_t row = 0; row < height; ++row) {
    std::memcpy(slot.pixels + static_cast<size_t>(y + row) * slot.width + x,
                bitmap + static_cast<size_t>(row) * width,
                static_cast<size_t>(width) * sizeof(uint16_t));
  }
  return true;
}

const lv_img_dsc_t *ArtworkManager::load(const std::string &url) {
  uint8_t *jpeg = nullptr;
  size_t length = 0;
  if (!download(url, jpeg, length)) {
    return nullptr;
  }
  const uint8_t target = active_slot_ == 0 ? 1 : 0;
  const bool decoded = decode(jpeg, length, slots_[target]);
  heap_caps_free(jpeg);
  if (!decoded) {
    return nullptr;
  }
  active_slot_ = target;
  return &slots_[active_slot_].image;
}

} // namespace spotctl

