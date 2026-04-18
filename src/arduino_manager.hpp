#pragma once

#include <asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <functional>

namespace rt::arduino {

class ArduinoManager {
public:
    using CommandCallback = std::function<void(const std::vector<unsigned char>& midiMessage)>;

    ArduinoManager(asio::io_context& io_context, std::string port_name, CommandCallback callback);
    ~ArduinoManager();

    void start();
    void stop();

private:
    void doRead();
    void processCommand(const std::string& line) const;

    asio::io_context& io_context_;
    std::string port_name_;
    asio::serial_port serial_;
    CommandCallback callback_;
    char read_buf_[1024];
    std::string line_buffer_;
    bool running_ = false;
};

} // namespace rt::arduino
