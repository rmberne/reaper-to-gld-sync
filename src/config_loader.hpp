#pragma once
#include <cstdint>
#include <string>

namespace config {

struct Config {
  std::string mixerIp = "192.168.1.10";
  std::string mixerPort = "51325";
  uint8_t midiChannel = 0;
  uint16_t nrpnParam = (0x42 << 7) | 0x01; // Slot 3, Param 1 (Time)
  bool mixerEnabled = true;
  bool pulseEnabled = false;
};

class ConfigLoader {
public:
  static Config load(const std::string &filename);
};

} // namespace config
