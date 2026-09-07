#include "ArtworkManager.h"

#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

#include "SpotifyRequest.h"

extern const uint8_t spotify_art_crt_bundle_start[]
    asm("_binary_x509_crt_bundle_start");
extern const uint8_t spotify_art_crt_bundle_end[]
    asm("_binary_x509_crt_bundle_end");

namespace spotctl {

namespace {
constexpr size_t kMaximumJpegBytes = 512U * 1024U;
// A row cover is a few kilobytes; anything larger is not a thumbnail.
constexpr size_t kMaximumThumbnailBytes = 64U * 1024U;
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
  TJpgDec.setSwapBytes(false);
  http_.setConnectTimeout(10000);
  http_.setTimeout(15000);
  http_.setReuse(true);
  http_.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
}

void ArtworkManager::configure(uint16_t player_dimension,
                               uint16_t thumbnail_dimension,
                               Stream &diagnostic) {
  player_dimension_ = player_dimension;
  thumbnail_dimension_ = thumbnail_dimension;
  diagnostic_ = &diagnostic;
}

void ArtworkManager::releaseConnection() {
  if (connected_host_.empty()) {
    return;
  }
  dropConnection();
}

void ArtworkManager::dropConnection() {
  http_.end();
  secure_client_.stop();
  connected_host_.clear();
}

bool ArtworkManager::download(const std::string &url, size_t byte_limit,
                              uint8_t *&data, size_t &length) {
  data = nullptr;
  length = 0;
  if (url.empty()) {
    return false;
  }
  // HTTPClient reuses a live socket without comparing hosts, so a cover served
  // from a different CDN must close the previous connection first.
  const std::string host = hostOf(url);
  if (host.empty()) {
    return false;
  }
  if (host != connected_host_) {
    dropConnection();
    connected_host_ = host;
  }
  if (!http_.begin(secure_client_, url.c_str())) {
    dropConnection();
    return false;
  }
  const int status = http_.GET();
  if (status != HTTP_CODE_OK) {
    if (diagnostic_ != nullptr) {
      diagnostic_->printf("[art] %s -> %d heap=%u largest=%u\n", url.c_str(),
                          status, static_cast<unsigned>(ESP.getFreeHeap()),
                          static_cast<unsigned>(heap_caps_get_largest_free_block(
                              MALLOC_CAP_INTERNAL)));
    }
    dropConnection();
    return false;
  }
  const int announced_length = http_.getSize();
  if (announced_length > static_cast<int>(byte_limit)) {
    dropConnection();
    return false;
  }
  const size_t capacity = announced_length > 0
                              ? static_cast<size_t>(announced_length)
                              : byte_limit;
  data = static_cast<uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM));
  if (data == nullptr) {
    dropConnection();
    return false;
  }

  NetworkClient *stream = http_.getStreamPtr();
  const uint32_t started = millis();
  while (http_.connected() && length < capacity && millis() - started < 15000) {
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

  const bool complete =
      length > 0 &&
      (announced_length <= 0 || length == static_cast<size_t>(announced_length));
  if (!complete) {
    // A partial body leaves unread bytes on the socket, which would desync the
    // next request, so this connection does not get reused.
    dropConnection();
    heap_caps_free(data);
    data = nullptr;
    length = 0;
    return false;
  }
  // Keeps the socket open for the next cover.
  http_.end();
  return true;
}

ArtworkHandle ArtworkManager::decode(uint8_t *jpeg, size_t length,
                                     uint16_t max_dimension) {
  uint16_t width = 0;
  uint16_t height = 0;
  if (TJpgDec.getJpgSize(&width, &height, jpeg, length) != JDR_OK || width == 0 ||
      height == 0 || width > kMaximumDimension || height > kMaximumDimension) {
    return {};
  }

  // The decoder can downscale as it goes, so a row cover never occupies full
  // resolution pixels even for a moment.
  uint8_t scale = 1;
  while (scale < 8 && (width / scale > max_dimension ||
                       height / scale > max_dimension)) {
    scale = static_cast<uint8_t>(scale * 2);
  }
  width = static_cast<uint16_t>(width / scale);
  height = static_cast<uint16_t>(height / scale);
  if (width == 0 || height == 0) {
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
  TJpgDec.setJpgScale(scale);
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

ArtworkHandle ArtworkManager::resampleSquare(const ArtworkHandle &source,
                                              uint16_t dimension) {
  if (!source || dimension == 0 ||
      (source->width == dimension && source->height == dimension)) {
    return source;
  }
  ArtworkHandle output = std::make_shared<ArtworkFrame>();
  output->pixels = static_cast<uint16_t *>(heap_caps_malloc(
      static_cast<size_t>(dimension) * dimension * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM));
  if (output->pixels == nullptr) {
    return {};
  }
  output->width = dimension;
  output->height = dimension;

  // Spotify covers are square. Center-crop defensively so unusual podcast
  // images fill the square without geometric distortion.
  const uint16_t crop = std::min(source->width, source->height);
  const uint16_t offset_x = static_cast<uint16_t>((source->width - crop) / 2);
  const uint16_t offset_y = static_cast<uint16_t>((source->height - crop) / 2);
  for (uint16_t y = 0; y < dimension; ++y) {
    const uint16_t source_y = static_cast<uint16_t>(
        offset_y + static_cast<uint32_t>(y) * crop / dimension);
    for (uint16_t x = 0; x < dimension; ++x) {
      const uint16_t source_x = static_cast<uint16_t>(
          offset_x + static_cast<uint32_t>(x) * crop / dimension);
      output->pixels[static_cast<size_t>(y) * dimension + x] =
          source->pixels[static_cast<size_t>(source_y) * source->width + source_x];
    }
  }
  output->image.header.always_zero = 0;
  output->image.header.cf = LV_IMG_CF_TRUE_COLOR;
  output->image.header.w = dimension;
  output->image.header.h = dimension;
  output->image.data_size =
      static_cast<uint32_t>(dimension) * dimension * sizeof(uint16_t);
  output->image.data = reinterpret_cast<const uint8_t *>(output->pixels);
  return output;
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

ArtworkHandle ArtworkManager::fetch(const std::string &url,
                                    uint16_t max_dimension,
                                    size_t byte_limit) {
  uint8_t *jpeg = nullptr;
  size_t length = 0;
  if (!download(url, byte_limit, jpeg, length)) {
    return {};
  }
  const uint16_t decode_limit =
      byte_limit == kMaximumThumbnailBytes ? max_dimension : kMaximumDimension;
  ArtworkHandle decoded = decode(jpeg, length, decode_limit);
  heap_caps_free(jpeg);
  return resampleSquare(decoded, max_dimension);
}

ArtworkHandle ArtworkManager::load(const std::string &url) {
  ArtworkHandle frame = fetch(url, player_dimension_, kMaximumJpegBytes);
  releaseConnection();
  return frame;
}

ArtworkHandle ArtworkManager::loadThumbnail(const std::string &url) {
  return fetch(url, thumbnail_dimension_, kMaximumThumbnailBytes);
}

} // namespace spotctl
