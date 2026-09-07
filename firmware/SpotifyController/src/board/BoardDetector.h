#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "BoardProfile.h"

namespace spotctl {

class BoardProfileStore {
public:
  BoardProfile load() const;
  bool save(BoardProfile profile) const;
};

class BoardDetector {
public:
  explicit BoardDetector(TwoWire &wire) : wire_(wire) {}

  BoardProfile detect();
  bool verify(BoardProfile profile);
  const DetectionEvidence &evidence() const { return evidence_; }

private:
  bool probeWide();
  bool probeCompact();
  bool write8(uint8_t address, uint8_t reg, uint8_t value);
  bool read8(uint8_t address, uint8_t reg, uint8_t *data, size_t length);
  bool read16(uint8_t address, uint16_t reg, uint8_t *data, size_t length);

  TwoWire &wire_;
  BoardProfileStore store_;
  DetectionEvidence evidence_;
};

} // namespace spotctl
