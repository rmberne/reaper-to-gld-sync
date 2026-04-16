#pragma once
#include <memory>
#include <vector>

namespace rt::midi {

  class PulseManager {
public:
    PulseManager();
    ~PulseManager();

    static void startConnection();
    void setBpm(int bpm) const;
    void sendStart() const;
    void sendStop() const;

private:
    struct Impl;
    Impl *impl_;
  };

} // namespace rt::midi
