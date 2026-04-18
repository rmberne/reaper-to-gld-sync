#include "engine.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

Engine::Engine() : io_context_() {
  work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(asio::make_work_guard(io_context_));
}

Engine::~Engine() { stop(); }

void Engine::start(const EngineConfig &config) {
  if (running_)
    return;

  spdlog::info("[Engine] Starting with configuration:");
  spdlog::info("  - Arduino Enabled: {}", config.arduinoEnabled ? "YES" : "NO");
  spdlog::info("  - Pulse Enabled: {}", config.pulseEnabled ? "YES" : "NO");
  spdlog::info("  - Mixer Enabled: {}", config.mixerEnabled ? "YES" : "NO");
  spdlog::info("  - Mixer IP: {}", config.mixerIp);
  spdlog::info("  - Mixer Port: {}", config.mixerPort);
  spdlog::info("  - MIDI Channel: {}", config.midiChannel);
  spdlog::info("  - NRPN Param: {}", config.nrpnParam);

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
        spdlog::info("[Engine] Clock Change -> Pulse: {}", current);
        pulse_->setBpm(current);
      }

      if (mixer_ && mixer_->isConnected()) {
        spdlog::info("[Engine] Clock Change -> Mixer: {} x {}", bpm, multiplier);
        mixer_->syncToBPM(bpm, multiplier);
      }
      lastBpm_ = current;
    });

    midi_manager_->setClockCallback([this](const unsigned char status) {
      if (status == 0xFA || status == 0xFB) {
        spdlog::info("[Transport] START");
        if (pulse_) {
          pulse_->setBpm(currentKnownBpm_);
          pulse_->sendStart();
        }
        if (mixer_ && mixer_->isConnected()) {
          mixer_->syncToBPM(static_cast<float>(currentKnownBpm_), 1.0f);
        }
        lastBpm_ = currentKnownBpm_;
      } else if (status == 0xFC) {
        spdlog::info("[Transport] STOP");
        if (pulse_)
          pulse_->sendStop();
        lastBpm_ = -1;
      }
    });

    if (!midi_manager_->openDefaultPort()) {
      spdlog::error("[Engine] Failed to open MIDI port");
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
      spdlog::info("[Engine] IO thread started");
      try {
        io_context_.run();
      } catch (const std::exception &e) {
        spdlog::error("[Engine] IO Context error: {}", e.what());
      }
      spdlog::info("[Engine] IO thread stopped");
    });

  } catch (const std::exception &e) {
    spdlog::error("[Engine] Start error: {}", e.what());
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

  spdlog::info("[Engine] Stopped");
}
