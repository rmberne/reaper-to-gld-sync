#pragma once
#include <RtMidi.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rt {
namespace midi {

class PulseManager; // Forward declaration

class MidiManager {
public:
  using SyncCallback = std::function<void(float bpm, float multiplier)>;
  using ClockCallback = std::function<void(unsigned char status)>;

  MidiManager(SyncCallback callback);
  ~MidiManager();

  bool openDefaultPort();
  void start();

  void setClockCallback(ClockCallback callback) { clockCallback_ = callback; }

private:
  static void staticCallback(double deltatime,
                             std::vector<unsigned char> *message,
                             void *userData);
  void handleMidiMessage(double deltatime, std::vector<unsigned char> *message);

  std::unique_ptr<RtMidiIn> midiin_;
  SyncCallback callback_;
  ClockCallback clockCallback_;

  float currentBPM_ = 120.0f;
  float multiplier_ = 1.0f;
  double clockIntervalSum_ = 0;
  int clockCount_ = 0;

  std::chrono::steady_clock::time_point lastQuarterNoteTime_;
  bool firstClock_ = true;
  int validBeats_ = 0;
};

} // namespace midi
} // namespace rt
