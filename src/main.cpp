#include <asio.hpp>
#include <iostream>
#include <memory>
#include "mixer.hpp"
#include "midi_manager.hpp"

int main() {
    try {
        asio::io_context io_context;
        
        // 1. Initialize Mixer with its own connection management
        auto mixer = std::make_shared<Mixer>(io_context, "192.168.1.10", "51325");
        mixer->startReconnectionThread();

        // 2. Initialize MIDI Manager with a handover callback
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

        // 4. Run io_context to keep main thread alive and handle any async tasks
        auto work = asio::make_work_guard(io_context);
        io_context.run();

    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
