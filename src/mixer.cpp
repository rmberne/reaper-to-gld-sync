#include "mixer.hpp"
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>

Mixer::Mixer(asio::io_context &io_context, std::string host, std::string port, uint8_t channel, uint16_t parameter) :
    io_context_(io_context), socket_(io_context), host_(std::move(host)), port_(std::move(port)), midiChannel_(channel), parameter_(parameter) {}

Mixer::~Mixer() {
  stopReconnectionThread();
  closeSocketIfOpen();
}

void Mixer::connect() {
  closeSocketIfOpen();
  asio::ip::tcp::resolver resolver(io_context_);
  const auto endpoints = resolver.resolve(host_, port_);
  asio::connect(socket_, endpoints);
  connected_ = true;
  spdlog::info("[Mixer] Connected to {}:{}", host_, port_);
}

void Mixer::startReconnectionThread() {
  if (running_)
    return;
  running_ = true;
  reconnectThread_ = std::make_unique<std::thread>(&Mixer::runReconnectLoop, this);
}

void Mixer::stopReconnectionThread() {
  running_ = false;
  if (reconnectThread_ && reconnectThread_->joinable()) {
    reconnectThread_->join();
    reconnectThread_.reset();
  }
}

void Mixer::runReconnectLoop() {
  while (running_) {
    if (!isConnected()) {
      try {
        connect();
      } catch (const std::exception &e) {
        connected_ = false;
        if (running_) {
          spdlog::error("[Mixer] Connection failed: {}. Retrying in 3s...", e.what());
        }
      }
    }

    // Sleep in small increments to respond quickly to shutdown
    for (int i = 0; i < 30 && running_; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

bool Mixer::isConnected() const { return connected_ && socket_.is_open(); }

void Mixer::sendNrpn(const uint8_t midiChannel, const uint16_t parameter, const uint16_t value) {
  if (!socket_.is_open()) {
    spdlog::error("[Mixer] Error: Socket is not open.");
    return;
  }

  const uint8_t pMsb = (parameter >> 7) & 0x7F;
  const uint8_t pLsb = parameter & 0x7F;
  const uint8_t vMsb = (value >> 7) & 0x7F;
  const uint8_t vLsb = value & 0x7F;

  const uint8_t status = 0xB0 | (midiChannel & 0x0F);

  std::vector<uint8_t> midiMsg = {status, 99, pMsb, status, 98, pLsb, status, 6, vMsb, status, 38, vLsb};

  std::vector<uint8_t> tcpPacket;
  tcpPacket.reserve(4 + midiMsg.size());
  tcpPacket.push_back(0xF0); // Encapsulation Start
  tcpPacket.push_back(0x02); // Data Type: MIDI
  tcpPacket.push_back(midiMsg.size()); // Length (12)
  tcpPacket.push_back(0x1F); // Header End
  tcpPacket.insert(tcpPacket.end(), midiMsg.begin(), midiMsg.end());

  try {
    asio::write(socket_, asio::buffer(tcpPacket));
  } catch (const asio::system_error &e) {
    connected_ = false;
    spdlog::error("[Mixer] Send error: {}", e.what());
  }
}

void Mixer::syncToBPM(const float bpm, const float multiplier) {
  if (bpm <= 0.0f)
    return;

  const float ms = 60000.0f / (bpm * multiplier);
  constexpr float MAX_DELAY_MS = 2000.0f;
  const float scaledValue = (ms / MAX_DELAY_MS) * 16383.0f;

  const uint16_t nrpnValue = static_cast<uint16_t>(std::clamp(scaledValue, 0.0f, 16383.0f));

  spdlog::info("[Sync] BPM: {} Multiplier: {} -> Delay: {}ms -> NRPN: {}", bpm, multiplier, ms, nrpnValue);

  if (isConnected()) {
    sendNrpn(midiChannel_, parameter_, nrpnValue);
  }
}

void Mixer::closeSocketIfOpen() {
  if (socket_.is_open()) {
    try {
      socket_.close();
      connected_ = false;
    } catch (...) {
    }
  }
}
