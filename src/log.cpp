#include "log.hpp"

/**
 * @brief Constructor for Log class that initializes the log file and log level.
 *
 * @param config Shared pointer to configuration settings which include log file
 * path and level.
 */
Log::Log(const std::shared_ptr<Config>& config)
    : config_(config), level_(Level::INFO) {
  FUCK(config_->modeToString());
  if (config_->runMode() == RunMode::server)
    mode_ = std::string("SERVER");
  else if (config_->runMode() == RunMode::agent)
    mode_ = std::string("AGENT");
  // Open the log file for appending.
  std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
  if (!logFile.is_open()) {
    // Print an error message if the log file cannot be opened.
    std::cerr << "Error opening log file: " << config_->log().file
              << ". Make sure the directory and file exist.\n";
  }

  // Set the log level based on the configuration.
  const auto& logLevel = config_->log().level;
  if (logLevel == "INFO") {
    level_ = Level::INFO;
  } else if (logLevel == "TRACE") {
    level_ = Level::TRACE;
  } else if (logLevel == "DEBUG") {
    level_ = Level::DEBUG;
  } else {
    // Print an error message if the log level is invalid.
    std::cerr << "Invalid log level: " << logLevel
              << ". It should be one of [INFO|TRACE|DEBUG].\n";
  }
}

/**
 * @brief Copy constructor for Log class.
 *
 * @param log Shared pointer to another Log instance to copy from.
 */
Log::Log(const std::shared_ptr<Log>& log)
    : config_(log->config_), level_(log->level_) {}

Log::~Log() {}  ///< Destructor for Log class.

/**
 * @brief Write a log message to the log file and console.
 *
 * @param message The log message to write.
 * @param level The log level of the message.
 */
void Log::write(const std::string& message, Level level) const {
  // Check if the message should be logged based on its level.
  if (level <= level_ || level == Level::ERROR) {
    // Open the log file for appending.
    std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
    if (logFile.is_open()) {
      // Get the current time and format it as a timestamp.
      auto now = std::time(nullptr);
      auto localTime = *std::localtime(&now);
      std::ostringstream timestampStream;
      timestampStream << std::put_time(&localTime, "%Y-%m-%d_%H:%M:%S");
      std::string timestamp = timestampStream.str();

      // Create a formatted log line.
      std::string line = timestamp + " [" + mode_ + "]" + " [" +
                         levelToString(level) + "] " + message + "\n";
      logFile << line;    // Write the line to the log file.
      std::cout << line;  // Also write the line to the console.
    } else {
      // Print an error message if the log file cannot be opened.
      std::cerr << "Error opening log file: " << config_->log().file
                << ". Make sure the directory and file exist.\n";
    }
  }
}

/**
 * @brief Convert log level enumeration to its string representation.
 *
 * @param level The log level to convert.
 * @return String representation of the log level.
 */
std::string Log::levelToString(Level level) {
  switch (level) {
    case Level::INFO:
      return "INFO";
    case Level::TRACE:
      return "TRACE";
    case Level::ERROR:
      return "ERROR";
    case Level::DEBUG:
      return "DEBUG";
    default:
      return "UNKNOWN";  // Return "UNKNOWN" for any unrecognized log level.
  }
}
