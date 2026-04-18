#include "arduino_manager.hpp"
#include <iostream>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/ioctl.h>
#include <termios.h>
#endif

namespace rt::arduino {

  ArduinoManager::ArduinoManager(asio::io_context &io_context, std::string port_name, CommandCallback callback) :
      io_context_(io_context), port_name_(std::move(port_name)), serial_(io_context), callback_(std::move(callback)), read_buf_{} {}

  ArduinoManager::~ArduinoManager() { stop(); }

  void ArduinoManager::start() {
    try {
      serial_.open(port_name_);
      serial_.set_option(asio::serial_port_base::baud_rate(115200));
      serial_.set_option(asio::serial_port_base::character_size(8));
      serial_.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
      serial_.set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
      serial_.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

#if defined(__APPLE__) || defined(__linux__)
      // Arduinos often reset on connection. DTR/RTS might need to be toggled or set.
      int fd = serial_.native_handle();
      int flags;
      ioctl(fd, TIOCMGET, &flags);
      flags |= TIOCM_DTR;
      flags |= TIOCM_RTS;
      ioctl(fd, TIOCMSET, &flags);
#endif

      running_ = true;
      std::cout << "[Arduino] Opened port: " << port_name_ << " at 115200 baud" << std::endl;
      doRead();
    } catch (const std::exception &e) {
      std::cerr << "[Arduino] Error opening serial port " << port_name_ << ": " << e.what() << std::endl;
    }
  }

  void ArduinoManager::stop() {
    running_ = false;
    if (serial_.is_open()) {
      serial_.close();
    }
  }

  void ArduinoManager::doRead() {
    if (!running_)
      return;

    serial_.async_read_some(asio::buffer(read_buf_, sizeof(read_buf_)), [this](const std::error_code ec, std::size_t bytes_transferred) {
      if (!ec) {
        for (std::size_t i = 0; i < bytes_transferred; ++i) {
          if (const char c = read_buf_[i]; c == '\n' || c == '\r') {
            if (!line_buffer_.empty()) {
              processCommand(line_buffer_);
              line_buffer_.clear();
            }
          } else {
            line_buffer_ += c;
          }
        }
        doRead();
      } else if (running_) {
        std::cerr << "[Arduino] Read error on " << port_name_ << ": " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
      }
    });
  }

  void ArduinoManager::processCommand(const std::string& line) const {
    std::cout << "[Arduino] Received command: " << line << std::endl;
    if (line.empty()) return;
    const char cmd = line[0];
    // Mapping based on typical Arduino char commands
    // 's' = start, 'x' = stop, 'm' = mute, 'n' = next tab, 'p' = prev tab

    std::vector<unsigned char> msg;
    bool valid = true;

    switch (cmd) {
      case 's': // Start / Stop
        std::cout << "[Arduino] Command: START/STOP" << std::endl;
        msg = {0xB0, 100, 127}; // CC 100, Value 127
        break;
      case 'm': // Mute tracks (Toggle) - Using CC 102 as a suggestion
        std::cout << "[Arduino] Command: MUTE" << std::endl;
        msg = {0xB0, 102, 127};
        break;
      case 'n': // Next Tab - Using CC 103 as a suggestion
        std::cout << "[Arduino] Command: NEXT TAB" << std::endl;
        msg = {0xB0, 103, 127};
        break;
      case 'p': // Prev Tab - Using CC 104 as a suggestion
        std::cout << "[Arduino] Command: PREV TAB" << std::endl;
        msg = {0xB0, 104, 127};
        break;
      default:
        valid = false;
        break;
    }

    if (valid && callback_) {
      callback_(msg);
    }
  }

} // namespace rt::arduino
