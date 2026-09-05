#include "ProvisioningValidation.h"

namespace spotctl {

namespace {
bool containsLineBreak(const std::string &value) {
  return value.find_first_of("\r\n") != std::string::npos;
}
} // namespace

std::string validateProvisioning(const ProvisioningFields &fields) {
  if (fields.ssid.empty() || fields.ssid.size() > 32) {
    return "invalid_wifi_ssid";
  }
  if (!fields.password.empty() &&
      (fields.password.size() < 8 || fields.password.size() > 63)) {
    return "invalid_wifi_password";
  }
  if (fields.client_id.size() < 16 || fields.client_id.size() > 128 ||
      containsLineBreak(fields.client_id)) {
    return "invalid_client_id";
  }
  if (fields.refresh_token.empty() || fields.refresh_token.size() > 2048 ||
      containsLineBreak(fields.refresh_token)) {
    return "invalid_refresh_token";
  }
  return {};
}

} // namespace spotctl
