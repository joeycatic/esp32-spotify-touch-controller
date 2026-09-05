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

ArtworkFrame::~ArtworkFrame() {
  if (pixels != nullptr) {
    heap_caps_free(pixels);
  }
}

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

ArtworkHandle ArtworkManager::decode(uint8_t *jpeg, size_t length) {
  uint16_t width = 0;
  uint16_t height = 0;
  if (TJpgDec.getJpgSize(&width, &height, jpeg, length) != JDR_OK || width == 0 ||
      height == 0 || width > kMaximumDimension || height > kMaximumDimension) {
    return {};
  }

  ArtworkHandle frame = std::make_shared<ArtworkFrame>();
  frame->pixels = static_cast<uint16_t *>(heap_caps_malloc(
      static_cast<size_t>(width) * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  if (frame->pixels == nullptr) {
    return {};
  }
  frame->width = width;
  frame->height = height;
  decode_frame_ = frame.get();
  decoding_instance_ = this;
  const JRESULT result = TJpgDec.drawJpg(0, 0, jpeg, length);
  decoding_instance_ = nullptr;
  decode_frame_ = nullptr;
  if (result != JDR_OK) {
    return {};
  }

  frame->image.header.always_zero = 0;
  frame->image.header.cf = LV_IMG_CF_TRUE_COLOR;
  frame->image.header.w = width;
  frame->image.header.h = height;
  frame->image.data_size =
      static_cast<uint32_t>(width) * height * sizeof(uint16_t);
  frame->image.data = reinterpret_cast<const uint8_t *>(frame->pixels);
  return frame;
}

bool ArtworkManager::jpegBlock(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t *bitmap) {
  ArtworkManager *manager = decoding_instance_;
  if (manager == nullptr || manager->decode_frame_ == nullptr) {
    return false;
  }
  ArtworkFrame &frame = *manager->decode_frame_;
  if (x < 0 || y < 0 || x + width > frame.width ||
      y + height > frame.height) {
    return false;
  }
  for (uint16_t row = 0; row < height; ++row) {
    std::memcpy(frame.pixels + static_cast<size_t>(y + row) * frame.width + x,
                bitmap + static_cast<size_t>(row) * width,
                static_cast<size_t>(width) * sizeof(uint16_t));
  }
  return true;
}

ArtworkHandle ArtworkManager::load(const std::string &url) {
  uint8_t *jpeg = nullptr;
  size_t length = 0;
  if (!download(url, jpeg, length)) {
    return {};
  }
  ArtworkHandle decoded = decode(jpeg, length);
  heap_caps_free(jpeg);
  return decoded;
}

} // namespace spotctl
