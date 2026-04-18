#include "engine.hpp"
#include <cmath>
#include <iostream>

Engine::Engine() : io_context_() {
  work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(asio::make_work_guard(io_context_));
}

Engine::~Engine() { stop(); }

void Engine::start(const EngineConfig &config) {
  if (running_)
    return;

  std::cout << "[Engine] Starting with configuration:" << std::endl;
  std::cout << "  - Arduino Enabled: " << (config.arduinoEnabled ? "YES" : "NO") << std::endl;
  std::cout << "  - Pulse Enabled: " << (config.pulseEnabled ? "YES" : "NO") << std::endl;
  std::cout << "  - Mixer Enabled: " << (config.mixerEnabled ? "YES" : "NO") << std::endl;
  std::cout << "  - Mixer IP: " << config.mixerIp << std::endl;
  std::cout << "  - Mixer Port: " << config.mixerPort << std::endl;
  std::cout << "  - MIDI Channel: " << config.midiChannel << std::endl;
  std::cout << "  - NRPN Param: " << config.nrpnParam << std::endl;

  try {
    if (config.pulseEnabled) {
      pulse_ = std::make_shared<PulseManager>();
      pulse_->startConnection();
    }

    if (config.mixerEnabled) {
      mixer_ = std::make_shared<Mixer>(io_context_, config.mixerIp, std::to_string(config.mixerPort), static_cast<uint8_t>(config.midiChannel),
                                       static_cast<uint16_t>(config.nrpnParam));
      mixer_->startReconnectionThread();
    }

    midi_manager_ = std::make_unique<MidiManager>([this](const float bpm, const float multiplier) {
      const int current = static_cast<int>(std::round(bpm * multiplier));
      currentKnownBpm_ = current;

      if (pulse_) {
        std::cout << "[Engine] Clock Change -> Pulse: " << current << std::endl;
        pulse_->setBpm(current);
      }

      if (mixer_ && mixer_->isConnected()) {
        std::cout << "[Engine] Clock Change -> Mixer: " << bpm << " x " << multiplier << std::endl;
        mixer_->syncToBPM(bpm, multiplier);
      }
      lastBpm_ = current;
    });

    midi_manager_->setClockCallback([this](const unsigned char status) {
      if (status == 0xFA || status == 0xFB) {
        std::cout << "[Transport] START" << std::endl;
        if (pulse_) {
          pulse_->setBpm(currentKnownBpm_);
          pulse_->sendStart();
        }
        if (mixer_ && mixer_->isConnected()) {
          mixer_->syncToBPM(static_cast<float>(currentKnownBpm_), 1.0f);
        }
        lastBpm_ = currentKnownBpm_;
      } else if (status == 0xFC) {
        std::cout << "[Transport] STOP" << std::endl;
        if (pulse_)
          pulse_->sendStop();
        lastBpm_ = -1;
      }
    });

    if (!midi_manager_->openDefaultPort()) {
      std::cerr << "[Engine] Failed to open MIDI port" << std::endl;
      return;
    }
    midi_manager_->start();

    if (config.arduinoEnabled) {
      arduino_ = std::make_shared<rt::arduino::ArduinoManager>(io_context_, [this](const std::vector<unsigned char> &msg) {
        if (midi_manager_)
          midi_manager_->sendMidiMessage(msg);
      });
      arduino_->start();
    }

    running_ = true;
    io_thread_ = std::thread([this]() {
      std::cout << "[Engine] IO thread started" << std::endl;
      try {
        io_context_.run();
      } catch (const std::exception &e) {
        std::cerr << "[Engine] IO Context error: " << e.what() << std::endl;
      }
      std::cout << "[Engine] IO thread stopped" << std::endl;
    });

  } catch (const std::exception &e) {
    std::cerr << "[Engine] Start error: " << e.what() << std::endl;
  }
}

void Engine::stop() {
  if (!running_)
    return;

  running_ = false;

  if (arduino_) {
    arduino_->stop();
  }
  if (mixer_) {
    mixer_->closeSocketIfOpen();
  }

  io_context_.stop();

  if (io_thread_.joinable()) {
    io_thread_.join();
  }

  // Reset io_context for potential restart
  io_context_.restart();

  arduino_.reset();
  mixer_.reset();
  pulse_.reset();
  midi_manager_.reset();

  std::cout << "[Engine] Stopped" << std::endl;
}
