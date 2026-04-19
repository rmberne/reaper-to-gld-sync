#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace rt::arduino {

  class ArduinoManager {
public:
    using CommandCallback = std::function<void(const std::vector<unsigned char> &midiMessage)>;

    ArduinoManager(asio::io_context &io_context, CommandCallback callback);
    ~ArduinoManager();

    void start();
    void stop();
    void startReconnectionThread();
    void stopReconnectionThread();
    void sendCommand(char cmd);

private:
    void doRead();
    void processCommand(const std::string &line) const;
    void runReconnectLoop();
    static std::string autoDetectArduinoPort();

    asio::io_context &io_context_;
    std::string port_name_;
    asio::serial_port serial_;
    CommandCallback callback_;
    char read_buf_[1024];
    std::string line_buffer_;
    std::atomic<bool> running_ = false;
    std::atomic<bool> connected_ = false;
    std::unique_ptr<std::thread> reconnectThread_;
  };

} // namespace rt::arduino
