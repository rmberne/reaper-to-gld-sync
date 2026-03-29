#include "config_loader.hpp"
#include "midi_manager.hpp"
#include "mixer.hpp"
#include <asio.hpp>
#include <iostream>
#include <memory>

int main() {
  try {
    asio::io_context io_context;

    // 1. Load Configuration
    auto config = config::ConfigLoader::load("config.txt");
    std::cout << "[Config] Mixer: " << config.mixerIp << ":" << config.mixerPort
              << " Channel: " << (int)config.midiChannel
              << " Parameter: " << config.nrpnParam << std::endl;

    // 2. Initialize Mixer
    auto mixer =
        std::make_shared<Mixer>(io_context, config.mixerIp, config.mixerPort,
                                config.midiChannel, config.nrpnParam);
    mixer->startReconnectionThread();

    // 3. Initialize MIDI Manager
    rt::midi::MidiManager midiManager([mixer](float bpm, float multiplier) {
      std::cout << "[Sync] BPM: " << bpm << " Multiplier: " << multiplier << "x"
                << std::endl;
      if (mixer->isConnected()) {
        mixer->syncToBPM(bpm, multiplier);
      }
    });

    // 4. Setup and Start MIDI
    std::cout << "[Main] Setting up MIDI..." << std::endl;
    if (!midiManager.openDefaultPort()) {
      std::cerr << "[Main] Failed to open MIDI port." << std::endl;
      return 1;
    }
    midiManager.start();

    std::cout << "[Main] System Active. Waiting for MIDI data..." << std::endl;

    auto work = asio::make_work_guard(io_context);
    io_context.run();

  } catch (const std::exception &e) {
    std::cerr << "[Main] Fatal Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
