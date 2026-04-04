#pragma once
#include <memory>
#include <vector>

namespace rt {
namespace midi {

class PulseManager {
public:
  PulseManager();
  ~PulseManager();

  bool openPort();
  void setBpm(int bpm);
  void sendClock();
  void sendStart();
  void sendStop();

private:
  struct Impl;
  Impl* impl_;
};

} // namespace midi
} // namespace rt
