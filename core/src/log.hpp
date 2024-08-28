#pragma once

#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <string>

#include "config.hpp"
#include "general.hpp"

/**
 * @brief A class for logging messages with different levels (INFO, TRACE,
 * ERROR, DEBUG).
 *
 * The Log class handles logging messages to a file and/or console based on the
 * configured log level. It provides facilities to create log instances, set
 * the log level, and write log messages.
 */
class Log : private Uncopyable {
 public:
  enum class Level { INFO, TRACE, ERROR, DEBUG };
  using pointer = std::shared_ptr<Log>;

  static pointer create(const std::shared_ptr<Config> &config) {
    return pointer(new Log(config));
  }

  explicit Log(const std::shared_ptr<Log> &log);
  ~Log();

  const std::shared_ptr<Config> &config() const { return config_; }
  Level level() const { return level_; }

  void write(const std::string &message, Level level) const;

 private:
  explicit Log(const std::shared_ptr<Config> &config);

  static std::string levelToString(Level level);

  const std::shared_ptr<Config> &config_;
  Level level_;
  std::string mode_;

  mutable std::mutex logMutex_;  // Mutex to protect write operations
};
