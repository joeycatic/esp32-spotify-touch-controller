#include <Arduino.h>

#include "src/board/Board.h"
#include "src/app/NetworkService.h"
#include "src/net/WifiManager.h"
#include "src/provision/DeviceProvisioner.h"
#include "src/storage/ConfigStore.h"
#include "src/ui/Ui.h"

spotctl::Board board;
spotctl::ConfigStore config_store;
spotctl::WifiManager wifi;
spotctl::DeviceProvisioner provisioner(config_store);
spotctl::NetworkService network_service(config_store);
spotctl::Ui ui(board, network_service);
spotctl::DeviceConfig device_config;

bool provisioning_mode = false;
bool wifi_was_connected = false;
bool configured_before_setup = false;
bool factory_reset_complete = false;
uint32_t boot_hold_started_ms = 0;
uint8_t last_reset_countdown = 255;

void setup() {
  Serial.begin(115200);
  delay(150);

  if (!board.begin()) {
    Serial.println("Hardware initialization failed");
    for (;;) {
      delay(1000);
    }
  }
  configured_before_setup = config_store.load(device_config);
  const bool boot_held = board.bootButtonHeld();
  provisioning_mode = !configured_before_setup || boot_held;
  if (boot_held && configured_before_setup) {
    boot_hold_started_ms = millis();
  }

  if (provisioning_mode) {
    ui.begin(true);
    provisioner.begin();
  } else {
    ui.begin(false);
    ui.showConnecting(device_config.ssid.c_str());
    wifi.begin(device_config.ssid, device_config.password);
  }
}

void loop() {
  board.tick();
  ui.tick();
  if (provisioning_mode) {
    if (configured_before_setup && !factory_reset_complete &&
        boot_hold_started_ms != 0 && board.bootButtonHeld()) {
      const uint32_t held_ms = millis() - boot_hold_started_ms;
      if (held_ms >= 3000 && held_ms < 10000) {
        const uint8_t remaining =
            static_cast<uint8_t>((10000U - held_ms + 999U) / 1000U);
        if (remaining != last_reset_countdown) {
          last_reset_countdown = remaining;
          ui.showFactoryResetCountdown(remaining);
        }
      } else if (held_ms >= 10000) {
        config_store.erase();
        factory_reset_complete = true;
        ui.showFactoryResetComplete();
      }
    } else if (!board.bootButtonHeld()) {
      boot_hold_started_ms = 0;
    }
    if (provisioner.poll()) {
      board.showBootMessage("Setup saved", "Restarting...");
      delay(600);
      ESP.restart();
    }
  } else {
    wifi.tick();
    const bool connected = wifi.connected();
    if (connected && !network_service.running()) {
      network_service.begin(device_config);
    }
    if (connected && !wifi_was_connected) {
      ui.showOnline();
    } else if (!connected && wifi_was_connected) {
      ui.showOffline();
    }
    wifi_was_connected = connected;

    spotctl::NetworkEvent event{spotctl::NetworkEventType::Status};
    while (network_service.pollEvent(event)) {
      ui.handle(event);
      if (event.type == spotctl::NetworkEventType::AuthorizationRequired) {
        provisioning_mode = true;
        provisioner.begin();
        break;
      }
    }
  }
  delay(2);
}
