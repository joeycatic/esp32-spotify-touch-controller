#include "ConfigStore.h"

#include <Preferences.h>

#include "../provision/ProvisioningValidation.h"

namespace spotctl {

namespace {
constexpr uint32_t kSchemaVersion = 1;

String key(char slot, const char *suffix) {
  String result(slot);
  result += suffix;
  return result;
}
} // namespace

bool ConfigStore::loadSlot(char slot, DeviceConfig &config) {
  Preferences preferences;
  if (!preferences.begin("spotctl", true)) {
    return false;
  }
  const String schema_key = key(slot, "_schema");
  if (preferences.getUInt(schema_key.c_str(), 0) != kSchemaVersion) {
    preferences.end();
    return false;
  }
  config.ssid = preferences.getString(key(slot, "_ssid").c_str(), "").c_str();
  config.password =
      preferences.getString(key(slot, "_pass").c_str(), "").c_str();
  config.client_id =
      preferences.getString(key(slot, "_client").c_str(), "").c_str();
  config.refresh_token =
      preferences.getString(key(slot, "_refresh").c_str(), "").c_str();
  preferences.end();

  return validateProvisioning({config.ssid, config.password, config.client_id,
                               config.refresh_token})
      .empty();
}

bool ConfigStore::load(DeviceConfig &config) {
  Preferences preferences;
  if (!preferences.begin("spotctl", true)) {
    return false;
  }
  const char active = preferences.getChar("active", 'a');
  preferences.end();
  if (loadSlot(active, config)) {
    return true;
  }
  return loadSlot(active == 'a' ? 'b' : 'a', config);
}

bool ConfigStore::save(const DeviceConfig &config) {
  if (!validateProvisioning({config.ssid, config.password, config.client_id,
                             config.refresh_token})
           .empty()) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin("spotctl", false)) {
    return false;
  }
  const char active = preferences.getChar("active", 'a');
  const char target = active == 'a' ? 'b' : 'a';
  preferences.remove(key(target, "_schema").c_str());
  const size_t password_written = preferences.putString(
      key(target, "_pass").c_str(), config.password.c_str());
  const bool password_ok = config.password.empty() || password_written > 0;
  const bool written =
      preferences.putString(key(target, "_ssid").c_str(), config.ssid.c_str()) >
          0 &&
      password_ok &&
      preferences.putString(key(target, "_client").c_str(),
                            config.client_id.c_str()) > 0 &&
      preferences.putString(key(target, "_refresh").c_str(),
                            config.refresh_token.c_str()) > 0 &&
      preferences.putUInt(key(target, "_schema").c_str(), kSchemaVersion) > 0;
  if (written) {
    preferences.putChar("active", target);
  }
  preferences.end();

  DeviceConfig verified;
  return written && loadSlot(target, verified) && verified.ssid == config.ssid &&
         verified.client_id == config.client_id &&
         verified.refresh_token == config.refresh_token;
}

bool ConfigStore::updateRefreshToken(const std::string &refresh_token) {
  DeviceConfig config;
  if (!load(config)) {
    return false;
  }
  config.refresh_token = refresh_token;
  return save(config);
}

void ConfigStore::erase() {
  Preferences preferences;
  if (preferences.begin("spotctl", false)) {
    preferences.clear();
    preferences.end();
  }
}

} // namespace spotctl
