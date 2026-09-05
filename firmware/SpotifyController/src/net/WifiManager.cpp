#include "WifiManager.h"

#include <WiFi.h>

#include "../core/RuntimePolicy.h"

namespace spotctl {

void WifiManager::begin(const std::string &ssid, const std::string &password) {
  ssid_ = ssid;
  password_ = password;
  retry_attempt_ = 0;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  connect();
}

void WifiManager::connect() {
  WiFi.begin(ssid_.c_str(), password_.c_str());
  state_ = WifiState::Connecting;
  attempt_started_ms_ = millis();
}

void WifiManager::tick() {
  if (WiFi.status() == WL_CONNECTED) {
    state_ = WifiState::Connected;
    retry_attempt_ = 0;
    return;
  }

  const uint32_t now = millis();
  if (state_ == WifiState::Connecting && now - attempt_started_ms_ >= 15000) {
    WiFi.disconnect();
    retry_at_ms_ = now + backoffMs(retry_attempt_++, esp_random() % 500);
    state_ = WifiState::Backoff;
  } else if (state_ == WifiState::Backoff &&
             static_cast<int32_t>(now - retry_at_ms_) >= 0) {
    connect();
  } else if (state_ == WifiState::Connected) {
    retry_at_ms_ = now + 2000;
    state_ = WifiState::Backoff;
  }
}

bool WifiManager::connected() const { return WiFi.status() == WL_CONNECTED; }

int32_t WifiManager::rssi() const { return connected() ? WiFi.RSSI() : -127; }

} // namespace spotctl
