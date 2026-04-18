#include "logger.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

namespace rt::logger {

  void init() {
    try {
      // Initialize thread pool for async logging
      // 8192 items in queue, 1 background thread
      spdlog::init_thread_pool(8192, 1);

      std::vector<spdlog::sink_ptr> sinks;

#ifndef NDEBUG
      const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      sinks.push_back(console_sink);
#endif

      // Rotating file sink: max 5MB, max 10 files
      const auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("reaper_gld_sync.log", 1024 * 1024 * 5, 10);
      sinks.push_back(rotating_sink);

      const auto logger =
          std::make_shared<spdlog::async_logger>("global", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

      spdlog::register_logger(logger);
      spdlog::set_default_logger(logger);

      spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
      spdlog::set_level(spdlog::level::info);

      spdlog::info("Logger initialized (Async, Rotating File)");
    } catch (const spdlog::spdlog_ex &ex) {
      fprintf(stderr, "Log initialization failed: %s\n", ex.what());
    }
  }

} // namespace rt::logger
