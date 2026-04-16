#include "pulse_manager.hpp"
#include <ableton/Link.hpp>
#include <iostream>

namespace rt::midi {

  struct PulseManager::Impl {
    ableton::Link link;
    double lastBpm;
    bool shouldBePlaying;

    Impl() : link(120.0), lastBpm(120.0), shouldBePlaying(false) {
      link.enable(true);

      // THE MISSING INGREDIENT:
      // This tells the Link SDK to actually broadcast our Start/Stop commands to the Soundbrenner App
      link.enableStartStopSync(true);

      link.setNumPeersCallback([this](const std::size_t num) {
        if (num > 0) {
          std::cout << "\n[Pulse Link] PEER FOUND. Synchronizing..." << std::endl;

          auto sessionState = link.captureAppSessionState();
          const auto hostTime = link.clock().micros();

          sessionState.setTempo(lastBpm, hostTime);
          sessionState.setIsPlaying(shouldBePlaying, hostTime);

          link.commitAppSessionState(sessionState);
        }
      });
    }
  };

  PulseManager::PulseManager() : impl_(new Impl()) {}
  PulseManager::~PulseManager() { delete impl_; }

  void PulseManager::startConnection() { std::cout << "[Pulse Link] Engine running. Waiting for Soundbrenner App..." << std::endl; }

  void PulseManager::setBpm(const int bpm) const {
    if (bpm <= 0)
      return;
    impl_->lastBpm = static_cast<double>(bpm);

    auto sessionState = impl_->link.captureAppSessionState();
    const auto hostTime = impl_->link.clock().micros();

    sessionState.setTempo(impl_->lastBpm, hostTime);

    // DAW is already playing
    if (impl_->shouldBePlaying) {
      sessionState.setIsPlaying(true, hostTime);
    }

    impl_->link.commitAppSessionState(sessionState);
    std::cout << "[Pulse Link] Set Tempo: " << bpm << std::endl;
  }

  void PulseManager::sendStart() const {
    impl_->shouldBePlaying = true;
    auto sessionState = impl_->link.captureAppSessionState();
    const auto hostTime = impl_->link.clock().micros();

    sessionState.setIsPlaying(true, hostTime);
    // Requesting beat 0 forces the app to jump to the start of the pulse cycle
    sessionState.requestBeatAtTime(0.0, hostTime, 1.0);

    impl_->link.commitAppSessionState(sessionState);
    std::cout << "[Pulse Link] Pushing PLAY state" << std::endl;
  }

  void PulseManager::sendStop() const {
    impl_->shouldBePlaying = false;
    auto sessionState = impl_->link.captureAppSessionState();
    const auto hostTime = impl_->link.clock().micros();

    sessionState.setIsPlaying(false, hostTime);
    impl_->link.commitAppSessionState(sessionState);
    std::cout << "[Pulse Link] Pushing STOP state" << std::endl;
  }

} // namespace rt::midi
