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
bool display_available = false;

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(150);

  display_available = board.begin();
  configured_before_setup = config_store.load(device_config);
  provisioning_mode = !display_available || !configured_before_setup ||
                      board.bootButtonHeld();

  if (!display_available) {
    const char *recovery =
        "[recovery] Hardware unavailable; provisioning is active on UART0";
    Serial.println(recovery);
    Serial0.println(recovery);
    provisioner.begin(board.primarySerial());
    return;
  }
  if (provisioning_mode) {
    ui.begin(true);
    provisioner.begin(board.primarySerial());
  } else {
    ui.begin(false);
    ui.showConnecting(device_config.ssid.c_str());
    wifi.begin(device_config.ssid, device_config.password);
  }
}

void loop() {
  board.tick();
  if (display_available) {
    ui.tick();
    if (ui.pollAction() == spotctl::UiAction::FactoryResetRequested) {
      config_store.erase();
      delay(300);
      ESP.restart();
    }
  }
  if (provisioning_mode) {
    if (provisioner.poll()) {
      if (display_available) {
        ui.showSetupSaved();
      }
      delay(600);
      ESP.restart();
    }
  } else {
    wifi.tick();
    const bool connected = wifi.connected();
    if (connected && !network_service.running()) {
      network_service.begin(device_config, board.capabilities().media,
                            board.primarySerial());
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
        provisioner.begin(board.primarySerial());
        break;
      }
    }
  }
  delay(2);
}
