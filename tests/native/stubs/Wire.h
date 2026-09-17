#pragma once

#include <cstdint>
#include <vector>

class TwoWire {
public:
  void beginTransmission(uint8_t address) {
    current_address = address;
    current_bytes.clear();
  }

  size_t write(uint8_t value) {
    current_bytes.push_back(value);
    return 1;
  }

  uint8_t endTransmission(bool) {
    transmissions.push_back({current_address, current_bytes});
    return 0;
  }

  struct Transmission {
    uint8_t address;
    std::vector<uint8_t> bytes;
  };

  std::vector<Transmission> transmissions;

private:
  uint8_t current_address{0};
  std::vector<uint8_t> current_bytes;
};

