#include "midi_manager.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <utility>


namespace rt::midi {

  MidiManager::MidiManager(SyncCallback callback) : callback_(std::move(callback)) {
    try {
      midiin_ = std::make_unique<RtMidiIn>();
    } catch (RtMidiError &error) {
      error.printMessage();
    }
  }

  MidiManager::~MidiManager() = default;

  bool MidiManager::openDefaultPort() const {
    if (!midiin_)
      return false;

    const unsigned int nPorts = midiin_->getPortCount();
    unsigned int portToOpen = -1;

    // 1. Try to find "ReaperSync" if it already exists (e.g. from IAC or another tool)
    for (unsigned int i = 0; i < nPorts; i++) {
      std::string name = midiin_->getPortName(i);
      std::cout << "  " << i << ": " << name << std::endl;
      if (name.find("ReaperSync") != std::string::npos) {
        portToOpen = i;
        break;
      }
    }

    // 2. Try to find "IAC" as a second priority
    if (portToOpen == -1) {
      for (unsigned int i = 0; i < nPorts; i++) {
        if (midiin_->getPortName(i).find("IAC") != std::string::npos) {
          portToOpen = i;
          break;
        }
      }
    }

    if (portToOpen != -1) {
      midiin_->openPort(portToOpen);
      std::cout << "[MIDI] Opened existing port: " << midiin_->getPortName(portToOpen) << std::endl;
    } else {
      // 3. Fallback: Create our own virtual port
      midiin_->openVirtualPort("ReaperSync");
      std::cout << "[MIDI] Created virtual port: ReaperSync" << std::endl;
    }

    return true;
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
