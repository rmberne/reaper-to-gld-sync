#include <asio.hpp>
#include <iostream>
#include <memory>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include "mixer.hpp"
#include "midi_manager.hpp"

struct Config {
    std::string mixerIp = "192.168.1.10";
    std::string mixerPort = "51325";
    uint8_t midiChannel = 0;
    uint16_t nrpnParam = (0x42 << 7) | 0x01; // Slot 3, Param 1 (Time)
};

Config loadConfig(const std::string& filename) {
    Config config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[Config] No config file found. Using defaults." << std::endl;
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                if (key == "IP") config.mixerIp = value;
                else if (key == "PORT") config.mixerPort = value;
                else if (key == "CHANNEL") config.midiChannel = static_cast<uint8_t>(std::stoi(value));
                else if (key == "PARAMETER") config.nrpnParam = static_cast<uint16_t>(std::stoi(value));
            }
        }
    }
    std::cout << "[Config] Loaded from " << filename << std::endl;
    return config;
}

int main() {
    try {
        asio::io_context io_context;
        
        Config config = loadConfig("config.txt");
        std::cout << "[Config] Mixer: " << config.mixerIp << ":" << config.mixerPort 
                  << " Channel: " << (int)config.midiChannel 
                  << " Parameter: " << config.nrpnParam << std::endl;

        // 1. Initialize Mixer
        auto mixer = std::make_shared<Mixer>(io_context, config.mixerIp, config.mixerPort, config.midiChannel, config.nrpnParam);
        mixer->startReconnectionThread();

        // 2. Initialize MIDI Manager
        rt::midi::MidiManager midiManager([mixer](float bpm, float multiplier) {
            std::cout << "[Sync] BPM: " << bpm << " Multiplier: " << multiplier << "x" << std::endl;
            if (mixer->isConnected()) {
                mixer->syncToBPM(bpm, multiplier);
            }
        });

        // 3. Setup and Start MIDI
        std::cout << "[Main] Setting up MIDI..." << std::endl;
        if (!midiManager.openDefaultPort()) {
            std::cerr << "[Main] Failed to open MIDI port." << std::endl;
            return 1;
        }
        midiManager.start();

        std::cout << "[Main] System Active. Waiting for MIDI data..." << std::endl;

        auto work = asio::make_work_guard(io_context);
        io_context.run();

    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
