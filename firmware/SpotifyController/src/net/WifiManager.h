#pragma once

#include <cstdint>
#include <string>

namespace spotctl {

enum class WifiState { Idle, Connecting, Connected, Backoff };

class WifiManager {
public:
  void begin(const std::string &ssid, const std::string &password);
  void tick();
  bool connected() const;
  WifiState state() const { return state_; }
  int32_t rssi() const;

private:
  void connect();

  std::string ssid_;
  std::string password_;
  WifiState state_{WifiState::Idle};
  uint32_t attempt_started_ms_{0};
  uint32_t retry_at_ms_{0};
  uint8_t retry_attempt_{0};
};

} // namespace spotctl

