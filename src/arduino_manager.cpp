#include "arduino_manager.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/ioctl.h>
#include <termios.h>
#endif

namespace rt::arduino {

  ArduinoManager::ArduinoManager(asio::io_context &io_context, CommandCallback callback) :
      io_context_(io_context), serial_(io_context), callback_(std::move(callback)), read_buf_{} {
    port_name_ = autoDetectArduinoPort();
  }

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
      const int fd = serial_.native_handle();
      int flags;
      ioctl(fd, TIOCMGET, &flags);
      flags |= TIOCM_DTR;
      flags |= TIOCM_RTS;
      ioctl(fd, TIOCMSET, &flags);
#endif

      running_ = true;
      spdlog::info("[Arduino] Opened port: {} at 115200 baud", port_name_);
      doRead();
    } catch (const std::exception &e) {
      spdlog::error("[Arduino] Error opening serial port {}: {}", port_name_, e.what());
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

    serial_.async_read_some(asio::buffer(read_buf_, sizeof(read_buf_)), [this](const std::error_code ec, const std::size_t bytes_transferred) {
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
        spdlog::error("[Arduino] Read error on {}: {} (code: {})", port_name_, ec.message(), ec.value());
      }
    });
  }

  void ArduinoManager::processCommand(const std::string &line) const {
    spdlog::info("[Arduino] Received command: {}", line);
    if (line.empty())
      return;
    const char cmd = line[0];
    // Mapping based on typical Arduino char commands
    // 's' = start, 'x' = stop, 'm' = mute, 'n' = next tab, 'p' = prev tab

    std::vector<unsigned char> msg;
    bool valid = true;

    switch (cmd) {
      case 's': // Start / Stop
        spdlog::info("[Arduino] Command: START/STOP");
        msg = {0xB0, 100, 127}; // CC 100, Value 127
        break;
      case 'm': // Mute tracks (Toggle) - Using CC 102 as a suggestion
        spdlog::info("[Arduino] Command: MUTE");
        msg = {0xB0, 102, 127};
        break;
      case 'n': // Next Tab - Using CC 103 as a suggestion
        spdlog::info("[Arduino] Command: NEXT TAB");
        msg = {0xB0, 103, 127};
        break;
      case 'p': // Prev Tab - Using CC 104 as a suggestion
        spdlog::info("[Arduino] Command: PREV TAB");
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

  std::string ArduinoManager::autoDetectArduinoPort() {
    constexpr std::string devPath = "/dev/";
    constexpr std::string modemPrefix = "cu.usbmodem";
    constexpr std::string serialPrefix = "cu.usbserial";

    try {
      for (const auto &entry: std::filesystem::directory_iterator(devPath)) {

        // If the file starts with cu.usbmodem or cu.usbserial, it's our Arduino!
        if (std::string filename = entry.path().filename().string(); filename.find(modemPrefix) == 0 || filename.find(serialPrefix) == 0) {
          spdlog::info("[Auto-Detect] Found Arduino at: {}", entry.path().string());
          return entry.path().string();
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("[Auto-Detect] Error scanning /dev/: {}", e.what());
    }

    spdlog::warn("[Auto-Detect] No Arduino found.");
    return ""; // Return empty string if nothing is found
  }

} // namespace rt::arduino
