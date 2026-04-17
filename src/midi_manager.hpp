#pragma once
#include <RtMidi.h>
#include <functional>
#include <memory>
#include <vector>

namespace rt::midi {

  class PulseManager; // Forward declaration

  class MidiManager {
public:
    using SyncCallback = std::function<void(float bpm, float multiplier)>;
    using ClockCallback = std::function<void(unsigned char status)>;

    explicit MidiManager(SyncCallback callback);
    ~MidiManager();

    [[nodiscard]] bool openDefaultPort() const;
    void start();

    void setClockCallback(const ClockCallback &callback) { clockCallback_ = callback; }

    void sendMidiMessage(const std::vector<unsigned char>& message) const;

private:
    static void staticCallback(double deltatime, std::vector<unsigned char> *message, void *userData);
    void handleMidiMessage(double deltatime, const std::vector<unsigned char> *message);

    std::unique_ptr<RtMidiIn> midiin_;
    std::unique_ptr<RtMidiOut> midiout_;
    SyncCallback callback_;
    ClockCallback clockCallback_;

    float currentBPM_ = 120.0f;
    float multiplier_ = 1.0f;
    double clockIntervalSum_ = 0;

    std::deque<long long> tickDeltas_;
    std::chrono::steady_clock::time_point lastTickTime_;
    bool firstClock_ = true;
  };

} // namespace rt::midi
