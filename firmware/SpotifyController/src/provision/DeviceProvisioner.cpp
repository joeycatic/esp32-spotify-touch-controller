#include "DeviceProvisioner.h"

#include <ArduinoJson.h>

#include "ProvisioningValidation.h"

namespace spotctl {

namespace {
constexpr size_t kMaximumMessageBytes = 4096;
} // namespace

void DeviceProvisioner::begin(Stream &serial) {
  serial_ = &serial;
  input_.reserve(kMaximumMessageBytes);
  input_.clear();
  overflowed_ = false;
}

bool DeviceProvisioner::poll() {
  if (serial_ == nullptr) {
    return false;
  }
  while (serial_->available() > 0) {
    const char character = static_cast<char>(serial_->read());
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      if (overflowed_) {
        respond(false, "payload_too_large", "Provisioning message is too large");
        overflowed_ = false;
        input_.clear();
        continue;
      }
      const bool saved = !input_.isEmpty() && processLine(input_);
      input_.clear();
      if (saved) {
        return true;
      }
      continue;
    }
    if (input_.length() >= kMaximumMessageBytes) {
      overflowed_ = true;
      continue;
    }
    if (!overflowed_) {
      input_ += character;
    }
  }
  return false;
}

bool DeviceProvisioner::processLine(const String &line) {
  JsonDocument document;
  const DeserializationError parse_error = deserializeJson(document, line);
  if (parse_error) {
    respond(false, "invalid_json", "Provisioning message is not valid JSON");
    return false;
  }
  if (document["v"].as<int>() != 1 ||
      String(document["type"].as<const char *>()) != "provision") {
    respond(false, "unsupported_protocol", "Unsupported provisioning protocol");
    return false;
  }

  DeviceConfig config;
  config.ssid = document["wifi"]["ssid"].as<const char *>() ?: "";
  config.password = document["wifi"]["password"].as<const char *>() ?: "";
  config.client_id = document["spotify"]["client_id"].as<const char *>() ?: "";
  config.refresh_token =
      document["spotify"]["refresh_token"].as<const char *>() ?: "";
  const std::string validation = validateProvisioning(
      {config.ssid, config.password, config.client_id, config.refresh_token});
  if (!validation.empty()) {
    respond(false, validation.c_str(), "Provisioning fields failed validation");
    return false;
  }
  if (!store_.save(config)) {
    respond(false, "storage_failed", "Could not save configuration");
    return false;
  }
  respond(true, "ok", "Configuration saved");
  return true;
}

void DeviceProvisioner::respond(bool ok, const char *code,
                                const char *message) {
  JsonDocument response;
  response["v"] = 1;
  response["type"] = "provision_result";
  response["ok"] = ok;
  response["code"] = code;
  response["message"] = message;
  if (serial_ == nullptr) {
    return;
  }
  serializeJson(response, *serial_);
  serial_->println();
  serial_->flush();
}

} // namespace spotctl
