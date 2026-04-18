#pragma once

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <thread>
#include "arduino_manager.hpp"
#include "midi_manager.hpp"
#include "mixer.hpp"
#include "pulse_manager.hpp"

struct EngineConfig {
  bool arduinoEnabled;
  bool pulseEnabled;
  bool mixerEnabled;
  std::string mixerIp;
  int mixerPort;
  int midiChannel;
  int nrpnParam;
};

class Engine {
  public:
  Engine();
  ~Engine();

  void start(const EngineConfig &config);
  void stop();
  bool isRunning() const { return running_; }

  private:
  asio::io_context io_context_;
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;

  std::shared_ptr<PulseManager> pulse_;
  std::shared_ptr<Mixer> mixer_;
  std::unique_ptr<MidiManager> midi_manager_;
  std::shared_ptr<rt::arduino::ArduinoManager> arduino_;

  std::thread io_thread_;
  std::atomic<bool> running_{false};

  int lastBpm_ = -1;
  int currentKnownBpm_ = 120;
};
