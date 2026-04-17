#include "midi_manager.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <utility>


namespace rt::midi {

  MidiManager::MidiManager(SyncCallback callback) : callback_(std::move(callback)) {
    try {
      midiin_ = std::make_unique<RtMidiIn>();
      midiout_ = std::make_unique<RtMidiOut>();
    } catch (RtMidiError &error) {
      error.printMessage();
    }
  }

  MidiManager::~MidiManager() = default;

  bool MidiManager::openDefaultPort() const {
    if (!midiin_ || !midiout_)
      return false;

    // --- MIDI Input Port Setup (C++ listens here) ---
    const unsigned int nInPorts = midiin_->getPortCount();
    bool inFound = false;

    for (unsigned int i = 0; i < nInPorts; i++) {
      if (std::string name = midiin_->getPortName(i); name.find("ReaperSync IN") != std::string::npos) {
        midiin_->openPort(i);
        std::cout << "[MIDI] Opened existing input port: " << name << std::endl;
        inFound = true;
        break;
      }
    }

    if (!inFound) {
      try {
        midiin_->openVirtualPort("ReaperSync IN");
        std::cout << "[MIDI] Created virtual input port: ReaperSync IN" << std::endl;
        inFound = true;
      } catch (RtMidiError &error) {
        // ... fallback to IAC ...
      }
    }

    // --- MIDI Output Port Setup (C++ talks here) ---
    const unsigned int nOutPorts = midiout_->getPortCount();
    bool outFound = false;

    for (unsigned int i = 0; i < nOutPorts; i++) {
      if (std::string name = midiout_->getPortName(i); name.find("ReaperSync OUT") != std::string::npos) {
        midiout_->openPort(i);
        std::cout << "[MIDI] Opened existing output port: " << name << std::endl;
        outFound = true;
        break;
      }
    }

    if (!outFound) {
      try {
        midiout_->openVirtualPort("ReaperSync OUT");
        std::cout << "[MIDI] Created virtual output port: ReaperSync OUT" << std::endl;
        outFound = true;
      } catch (RtMidiError &error) {
        // ... fallback to IAC ...
      }
    }

    return inFound && outFound;
  }

  void MidiManager::sendMidiMessage(const std::vector<unsigned char> &message) const {
    if (!midiout_) {
      std::cerr << "[MIDI OUT] Failed: midiout_ pointer is null!" << std::endl;
      return;
    }

    // Mac/CoreMIDI Quirk: Virtual ports sometimes report as "not open".
    // We log it as a warning but attempt to send it anyway!
    if (!midiout_->isPortOpen()) {
      std::cout << "[MIDI OUT] Warning: isPortOpen() returned false, sending anyway..." << std::endl;
    }

    try {
      midiout_->sendMessage(&message);

    } catch (const RtMidiError &error) {
      std::cerr << "[MIDI OUT] RtMidi Error: " << error.getMessage() << std::endl;
    }
  }

  void MidiManager::start() {
    if (!midiin_)
      return;
    midiin_->setCallback(&MidiManager::staticCallback, this);
    midiin_->ignoreTypes(false, false, false);
  }

  void MidiManager::staticCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    static_cast<MidiManager *>(userData)->handleMidiMessage(deltatime, message);
  }

  void MidiManager::handleMidiMessage(double deltatime, const std::vector<unsigned char> *message) {
    if (message->empty())
      return;

    const unsigned char statusByte = message->at(0);
    const unsigned char status = statusByte & 0xF0;

    // --- NEW: Trigger external Clock Callback for Transport (main.cpp) ---
    if (clockCallback_ && (statusByte == 0xFA || statusByte == 0xFB || statusByte == 0xFC)) {
      clockCallback_(statusByte);
    }

    // --- Handle Transport Start/Continue/Stop ---
    if (statusByte == 0xFA || statusByte == 0xFB || statusByte == 0xFC) {
      tickDeltas_.clear(); // Wipe the sliding window clean
      firstClock_ = true;
      return;
    }

    // 1. MIDI Clock (0xF8)
    if (statusByte == 0xF8) {
      const auto now = std::chrono::steady_clock::now();

      // Establish timestamp baseline on the first tick
      if (firstClock_) {
        lastTickTime_ = now;
        firstClock_ = false;
        return;
      }

      // Calculate exact microseconds since the very last tick
      const long long deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTickTime_).count();
      lastTickTime_ = now;

      // Timeout check: If it's been > 1 second since the last tick, the transport stopped.
      if (deltaUs > 1000000) {
        tickDeltas_.clear();
        return;
      }

      // Add to sliding window
      tickDeltas_.push_back(deltaUs);
      if (tickDeltas_.size() > 24) { // Keep window size strictly at 1 quarter note
        tickDeltas_.pop_front();
      }

      // We can confidently estimate BPM after just 6 ticks (1/16th note)
      if (tickDeltas_.size() >= 6) {
        const long long sum = std::accumulate(tickDeltas_.begin(), tickDeltas_.end(), 0LL);
        const double avgDelta = static_cast<double>(sum) / tickDeltas_.size();

        // 60 seconds * 1,000,000 microseconds / (24 PPQN * average delta)
        const float rawBpm = 60000000.0f / (24.0f * avgDelta);

        // Snap to integer BPM to kill the remaining USB jitter

        if (const float snappedBpm = std::round(rawBpm); std::abs(snappedBpm - currentBPM_) > 0.1f) {
          currentBPM_ = snappedBpm;
          if (callback_)
            callback_(currentBPM_, multiplier_);
        }
      }
      return;
    }

    if (message->size() < 3)
      return;
    const unsigned char data1 = message->at(1);
    const unsigned char data2 = message->at(2);

    bool changed = false;

    // 2. Multiplier (Note On)
    if (status == 0x90 && data2 > 0) {
      if (data1 == 60) {
        multiplier_ = 1.0f;
        changed = true;
      } else if (data1 == 61) {
        multiplier_ = 0.5f;
        changed = true;
      } else if (data1 == 62) {
        multiplier_ = 2.0f;
        changed = true;
      }
    }
    // 3. CC (Manual Tempo Override)
    else if (status == 0xB0 && data1 == 20) {
      currentBPM_ = static_cast<float>(data2) + 60.0f;
      changed = true;
    }

    if (changed && callback_) {
      callback_(currentBPM_, multiplier_);
    }
  }

} // namespace rt::midi
