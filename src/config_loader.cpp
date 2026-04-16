#include "config_loader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace config {

Config ConfigLoader::load(const std::string &filename) {
  Config config;
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cout << "[Config] No config file found. Using defaults." << std::endl;
    return config;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Simple trim for potential whitespace/carriage returns
    line.erase(line.find_last_not_of(" \n\r\t") + 1);

    std::istringstream is_line(line);
    if (std::string key; std::getline(is_line, key, '=')) {
      if (std::string value; std::getline(is_line, value)) {
        try {
          if (key == "IP")
            config.mixerIp = value;
          else if (key == "PORT")
            config.mixerPort = value;
          else if (key == "CHANNEL")
            config.midiChannel = static_cast<uint8_t>(std::stoi(value));
          else if (key == "PARAMETER")
            config.nrpnParam = static_cast<uint16_t>(std::stoi(value));
          else if (key == "MIXER_ENABLED")
            config.mixerEnabled = (value == "true" || value == "1");
          else if (key == "PULSE_ENABLED")
            config.pulseEnabled = (value == "true" || value == "1");
        } catch (const std::exception &e) {
          std::cerr << "[Config] Error parsing line '" << line
                    << "': " << e.what() << std::endl;
        }
      }
    }
  }
  std::cout << "[Config] Loaded from " << filename << std::endl;
  return config;
}

} // namespace config
