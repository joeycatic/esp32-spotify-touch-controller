#pragma once

#include <Arduino.h>

#include "../storage/ConfigStore.h"

namespace spotctl {

class DeviceProvisioner {
public:
  explicit DeviceProvisioner(ConfigStore &store) : store_(store) {}

  void begin();
  bool poll();

private:
  bool processLine(const String &line);
  void respond(bool ok, const char *code, const char *message);

  ConfigStore &store_;
  String input_;
  bool overflowed_{false};
};

} // namespace spotctl
