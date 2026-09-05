#include <Arduino.h>

#include "src/board/Board.h"
#include "src/net/WifiManager.h"
#include "src/provision/DeviceProvisioner.h"
#include "src/storage/ConfigStore.h"

spotctl::Board board;
spotctl::ConfigStore config_store;
spotctl::WifiManager wifi;
spotctl::DeviceProvisioner provisioner(config_store);
spotctl::DeviceConfig device_config;

bool provisioning_mode = false;

void setup() {
  Serial.begin(115200);
  delay(150);

  board.begin();
  const bool configured = config_store.load(device_config);
  provisioning_mode = !configured || board.bootButtonHeld();

  if (provisioning_mode) {
    board.showBootMessage("USB setup", "Run: make provision");
    provisioner.begin();
  } else {
    board.showBootMessage("Connecting", device_config.ssid.c_str());
    wifi.begin(device_config.ssid, device_config.password);
  }
}

void loop() {
  board.tick();
  if (provisioning_mode) {
    if (provisioner.poll()) {
      board.showBootMessage("Setup saved", "Restarting...");
      delay(600);
      ESP.restart();
    }
  } else {
    wifi.tick();
  }
  delay(2);
}
