#include <asio.hpp>
#include <iostream>
#include <memory>
#include "config_loader.hpp"
#include "midi_manager.hpp"
#include "mixer.hpp"
#include "pulse_manager.hpp"

int main() {
  try {
    asio::io_context io_context;
    auto [mixerIp, mixerPort, midiChannel, nrpnParam, mixerEnabled, pulseEnabled] = config::ConfigLoader::load("config.txt");

    std::shared_ptr<PulseManager> pulse;
    if (pulseEnabled) {
      pulse = std::make_shared<PulseManager>();
      pulse->startConnection();
    }

    std::shared_ptr<Mixer> mixer;
    if (mixerEnabled) {
      mixer = std::make_shared<Mixer>(io_context, mixerIp, mixerPort, midiChannel, nrpnParam);
      mixer->startReconnectionThread();
    }

    int lastBpm = -1;
    int currentKnownBpm = 120; // Default fallback

    MidiManager midiManager([&](const float bpm, const float multiplier) {
      const int current = static_cast<int>(std::round(bpm * multiplier));
      currentKnownBpm = current; // Continuously track the latest BPM

      if (pulse) {
        std::cout << "[Main] Clock Change -> Pushing to Pulse: " << current << std::endl;
        pulse->setBpm(current);
      }

      if (mixer && mixer->isConnected()) {
        std::cout << "[Main] Clock Change -> Syncing Mixer: " << bpm << " x " << multiplier << std::endl;
        mixer->syncToBPM(bpm, multiplier);
      }
      lastBpm = current;
    });

    midiManager.setClockCallback([pulse, mixer, &currentKnownBpm, &lastBpm](const unsigned char status) {
      if (status == 0xFA || status == 0xFB) { // START or CONTINUE
        std::cout << "[Transport] START" << std::endl;
        if (pulse) {
          // 1. First set the tempo
          pulse->setBpm(currentKnownBpm);
          // 2. Then explicitly trigger the play state
          pulse->sendStart();
        }
        if (mixer && mixer->isConnected()) {
          mixer->syncToBPM(static_cast<float>(currentKnownBpm), 1.0f);
        }
        lastBpm = currentKnownBpm;
      } else if (status == 0xFC) { // STOP
        std::cout << "[Transport] STOP" << std::endl;
        if (pulse)
          pulse->sendStop();
        lastBpm = -1;
      }
    });

    if (!midiManager.openDefaultPort())
      return 1;
    midiManager.start();

    auto work = asio::make_work_guard(io_context);
    io_context.run();
  } catch (const std::exception &e) {
    std::cerr << "[Main] Fatal Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
