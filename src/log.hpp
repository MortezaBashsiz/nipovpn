#pragma once

#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>

#include "config.hpp"
#include "general.hpp"

/**
 * @brief A class for logging messages with different levels (INFO, TRACE,
 * ERROR, DEBUG).
 *
 * The Log class handles logging messages to a file and/or console based on the
 * configured log level. It provides facilities to create log instances, set the
 * log level, and write log messages.
 */
class Log : private Uncopyable {
 public:
  /**
   * @brief Enum for log levels.
   *
   * Defines the different levels of logging: INFO, TRACE, ERROR, and DEBUG.
   */
  enum class Level { INFO, TRACE, ERROR, DEBUG };

  using pointer =
      std::shared_ptr<Log>;  ///< Type alias for a shared pointer to Log.

  /**
   * @brief Factory method to create a shared pointer to a Log instance.
   *
   * @param config Shared pointer to configuration settings.
   * @return A shared pointer to a newly created Log instance.
   */
  static pointer create(const std::shared_ptr<Config>& config) {
    return pointer(new Log(config));
  }

  /**
   * @brief Copy constructor for Log class.
   *
   * @param log Shared pointer to another Log instance to copy from.
   */
  explicit Log(const std::shared_ptr<Log>& log);

  ~Log();  ///< Destructor for Log class.

  /**
   * @brief Get the configuration used by this Log instance.
   *
   * @return A const reference to the shared pointer to Config.
   */
  const std::shared_ptr<Config>& config() const { return config_; }

  /**
   * @brief Get the current log level.
   *
   * @return The log level as an enum value.
   */
  Level level() const { return level_; }

  /**
   * @brief Write a log message to the log file and/or console.
   *
   * @param message The log message to write.
   * @param level The log level of the message.
   */
  void write(const std::string& message, Level level) const;

 private:
  /**
   * @brief Constructor for Log class that initializes with configuration
   * settings.
   *
   * @param config Shared pointer to configuration settings.
   */
  explicit Log(const std::shared_ptr<Config>& config);

  /**
   * @brief Convert log level enumeration to its string representation.
   *
   * @param level The log level to convert.
   * @return String representation of the log level.
   */
  static std::string levelToString(Level level);

  const std::shared_ptr<Config>&
      config_;   ///< Configuration settings for the log.
  Level level_;  ///< Current log level.
};
