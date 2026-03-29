#include "midi_manager.hpp"
#include <iostream>
#include <cmath>
#include <chrono>

namespace rt { namespace midi {

MidiManager::MidiManager(SyncCallback callback) 
    : callback_(callback) {
    try {
        midiin_ = std::make_unique<RtMidiIn>();
    } catch (RtMidiError &error) {
        error.printMessage();
    }
}

MidiManager::~MidiManager() {}

bool MidiManager::openDefaultPort() {
    if (!midiin_) return false;

    unsigned int nPorts = midiin_->getPortCount();
    if (nPorts > 0) {
        unsigned int port = 0;
        for (unsigned int i = 0; i < nPorts; i++) {
            std::string name = midiin_->getPortName(i);
            std::cout << "  " << i << ": " << name << std::endl;
            if (name.find("IAC") != std::string::npos) port = i;
        }
        midiin_->openPort(port);
        std::cout << "[MIDI] Opened: " << midiin_->getPortName(port) << std::endl;
    } else {
        midiin_->openVirtualPort("ReaperSync");
        std::cout << "[MIDI] Opened virtual port: ReaperSync" << std::endl;
    }
    return true;
}

void MidiManager::start() {
    if (!midiin_) return;
    midiin_->setCallback(&MidiManager::staticCallback, this);
    midiin_->ignoreTypes(false, false, false);
}

void MidiManager::staticCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    static_cast<MidiManager*>(userData)->handleMidiMessage(deltatime, message);
}

void MidiManager::handleMidiMessage(double deltatime, std::vector<unsigned char> *message) {
    if (message->empty()) return;

    unsigned char statusByte = message->at(0);
    unsigned char status = statusByte & 0xF0;

    // --- NEW: Handle Transport Start/Continue/Stop ---
    // 0xFA = Start, 0xFB = Continue, 0xFC = Stop
    if (statusByte == 0xFA || statusByte == 0xFB || statusByte == 0xFC) {
        firstClock_ = true;
        clockCount_ = 0;
        validBeats_ = 0; // Reset the warm-up period
        return;
    }

    // 1. MIDI Clock (0xF8)
    if (statusByte == 0xF8) {
        clockCount_++;

        if (clockCount_ >= 24) { // One quarter note
            auto now = std::chrono::steady_clock::now();

            // Timeout check: If it's been more than 2 seconds since the last beat, 
            // the transport probably stopped. Reset the math.
            if (!firstClock_) {
                std::chrono::duration<float> idleTime = now - lastQuarterNoteTime_;
                if (idleTime.count() > 2.0f) {
                    firstClock_ = true;
                    validBeats_ = 0;
                }
            }

            // Ignore the very first calculation to establish a baseline timestamp
            if (firstClock_) {
                lastQuarterNoteTime_ = now;
                firstClock_ = false;
                clockCount_ = 0;
                return;
            }

            // Calculate exact absolute time passed since the last 24th pulse
            std::chrono::duration<float> elapsed = now - lastQuarterNoteTime_;
            float rawBpm = 60.0f / elapsed.count();

            // Snap to the nearest 0.5 BPM to kill jitter
            float snappedBpm = std::round(rawBpm * 2.0f) / 2.0f;

            // Warm-up Period ---
            // Wait for 2 stable quarter notes before trusting the data
            if (validBeats_ < 2) {
                validBeats_++;
            } else {
                // Now we are stable, trigger the callback to the mixer
                if (std::abs(snappedBpm - currentBPM_) > 0.1f) {
                    currentBPM_ = snappedBpm;
                    if (callback_) callback_(currentBPM_, multiplier_);
                }
            }

            // Reset for the next quarter note
            lastQuarterNoteTime_ = now;
            clockCount_ = 0;
        }
        return;
    }

    if (message->size() < 3) return;
    unsigned char data1 = message->at(1);
    unsigned char data2 = message->at(2);

    bool changed = false;

    // 2. Multiplier (Note On)
    if (status == 0x90 && data2 > 0) {
        if (data1 == 60) { multiplier_ = 1.0f; changed = true; }
        else if (data1 == 61) { multiplier_ = 0.5f; changed = true; }
        else if (data1 == 62) { multiplier_ = 2.0f; changed = true; }
    }
    // 3. CC (Manual Tempo Override)
    else if (status == 0xB0 && data1 == 20) {
        currentBPM_ = static_cast<float>(data2) + 60.0f; 
        changed = true;
    }

    if (changed && callback_ && validBeats_ >= 2) { // Only allow changes if warmed up
        callback_(currentBPM_, multiplier_);
    }
}

} } // namespace rt::midi
