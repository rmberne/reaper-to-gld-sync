#include "mixer.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

Mixer::Mixer(asio::io_context &io_context, std::string_view host,
             std::string_view port)
    : io_context_(io_context), socket_(io_context), host_(host), port_(port) {}

Mixer::~Mixer() {
  if (reconnectThread_ && reconnectThread_->joinable()) {
    // In a real app we'd need a stop flag, but for now we'll just detach or let
    // it die
    reconnectThread_->detach();
  }
  closeSocketIfOpen();
}

void Mixer::connect() {
  closeSocketIfOpen();
  asio::ip::tcp::resolver resolver(io_context_);
  auto endpoints = resolver.resolve(host_, port_);
  asio::connect(socket_, endpoints);
  connected_ = true;
  std::cout << "[Mixer] Connected to " << host_ << ":" << port_ << std::endl;
}

void Mixer::startReconnectionThread() {
  reconnectThread_ =
      std::make_unique<std::thread>(&Mixer::runReconnectLoop, this);
}

void Mixer::runReconnectLoop() {
  while (true) {
    if (!isConnected()) {
      try {
        connect();
      } catch (const std::exception &e) {
        connected_ = false;
        std::cerr << "[Mixer] Connection failed: " << e.what()
                  << ". Retrying in 3s..." << std::endl;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}

bool Mixer::isConnected() const { return connected_ && socket_.is_open(); }

void Mixer::sendNrpn(uint8_t midiChannel, uint16_t parameter, uint16_t value) {
  if (!socket_.is_open()) {
    std::cerr << "[Mixer] Error: Socket is not open." << std::endl;
    return;
  }

  // Calculate 7-bit parts
  uint8_t pMsb = (parameter >> 7) & 0x7F;
  uint8_t pLsb = parameter & 0x7F;
  uint8_t vMsb = (value >> 7) & 0x7F;
  uint8_t vLsb = value & 0x7F;

  // Control Change status byte: 0xB0 | Channel (0-15)
  uint8_t status = 0xB0 | (midiChannel & 0x0F);

  // 1. Prepare 12 bytes of MIDI CC data (NRPN sequence)
  std::vector<uint8_t> midiMsg = {status, 99, pMsb, status, 98, pLsb,
                                  status, 6,  vMsb, status, 38, vLsb};

  // 2. Wrap in GLD TCP Header
  std::vector<uint8_t> tcpPacket;
  tcpPacket.reserve(4 + midiMsg.size());
  tcpPacket.push_back(0xF0);           // Encapsulation Start
  tcpPacket.push_back(0x02);           // Data Type: MIDI
  tcpPacket.push_back(midiMsg.size()); // Length (12)
  tcpPacket.push_back(0x1F);           // Header End

  // 3. Append the MIDI data
  tcpPacket.insert(tcpPacket.end(), midiMsg.begin(), midiMsg.end());

  try {
    asio::write(socket_, asio::buffer(tcpPacket));
  } catch (const asio::system_error &e) {
    connected_ = false; // Mark for reconnection loop to pick it up
    std::cerr << "[Mixer] Send error: " << e.what() << std::endl;
  }
}

void Mixer::syncToBPM(float bpm, float multiplier) {
  if (bpm <= 0.0f)
    return;

  // Logic: ms = 60000 / (BPM * Multiplier)
  float ms = 60000.0f / (bpm * multiplier);

  // Convert ms to a 14-bit MIDI NRPN value (scale 0-16383)
  // Assuming 2000ms is the max range of the Delay FX
  const float MAX_DELAY_MS = 2000.0f;
  float scaledValue = (ms / MAX_DELAY_MS) * 16383.0f;

  uint16_t nrpnValue =
      static_cast<uint16_t>(std::clamp(scaledValue, 0.0f, 16383.0f));

  std::cout << "[Sync] BPM: " << bpm << " Multiplier: " << multiplier
            << " -> Delay: " << ms << "ms -> NRPN: " << nrpnValue << std::endl;

  if (isConnected()) {
    // Slot 3 MSB is 0x42, Param LSB is 0x01 (Time)
    uint16_t gldParameter = (0x42 << 7) | 0x01;
    sendNrpn(0, gldParameter, nrpnValue);
  }
}

void Mixer::closeSocketIfOpen() {
  if (socket_.is_open()) {
    try {
      socket_.close();
    } catch (const std::exception &e) {
      std::cerr << "[Mixer] Socket close warning: " << e.what() << std::endl;
    }
  }
}
