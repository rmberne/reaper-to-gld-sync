#pragma once
#include <asio.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Mixer {
public:
  Mixer(asio::io_context &io_context, std::string_view host,
        std::string_view port);
  ~Mixer();

  // Establishes a TCP connection to the mixer
  void connect();

  // Sends a 14-bit MIDI NRPN (Non-Registered Parameter Number)
  // Parameter and Value are both 14-bit (0-16383)
  void sendNrpn(uint8_t midiChannel, uint16_t parameter, uint16_t value);

  // Calculates and sends sync message to the mixer
  void syncToBPM(float bpm, float multiplier);

  // Starts a background thread that ensures the mixer stays connected
  void startReconnectionThread();

  bool isConnected() const;

private:
  asio::io_context &io_context_;
  asio::ip::tcp::socket socket_;
  std::string host_;
  std::string port_;
  std::atomic<bool> connected_{false};
  std::unique_ptr<std::thread> reconnectThread_;
  void runReconnectLoop();
  void closeSocketIfOpen();
};
