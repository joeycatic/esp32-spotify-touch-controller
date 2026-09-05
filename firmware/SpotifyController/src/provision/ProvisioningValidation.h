#pragma once

#include <string>

namespace spotctl {

struct ProvisioningFields {
  std::string ssid;
  std::string password;
  std::string client_id;
  std::string refresh_token;
};

// Empty means valid; otherwise returns a stable machine-readable error code.
std::string validateProvisioning(const ProvisioningFields &fields);

} // namespace spotctl
