#pragma once

#include <string>

namespace spotctl {

struct DeviceConfig {
  std::string ssid;
  std::string password;
  std::string client_id;
  std::string refresh_token;
};

class ConfigStore {
public:
  bool load(DeviceConfig &config);
  bool save(const DeviceConfig &config);
  bool updateRefreshToken(const std::string &refresh_token);
  void erase();

private:
  bool loadSlot(char slot, DeviceConfig &config);
};

} // namespace spotctl

