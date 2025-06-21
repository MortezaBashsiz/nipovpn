#include "log.hpp"

/**
 * @brief Constructs a `Log` object with the given configuration.
 *
 * Initializes the logging mode and log level based on the configuration.
 * Also verifies if the log file is accessible.
 *
 * @param config Shared pointer to the configuration object.
 */
Log::Log(const std::shared_ptr<Config> &config)
    : config_(config), level_(Level::INFO) {
    if (config_->runMode() == RunMode::server)
        mode_ = std::string("SERVER");
    else if (config_->runMode() == RunMode::agent)
        mode_ = std::string("AGENT");

    std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error opening log file: " << config_->log().file
                  << ". Make sure the directory and file exist.\n";
    }

    const auto &logLevel = config_->log().level;
    if (logLevel == "INFO") {
        level_ = Level::INFO;
    } else if (logLevel == "TRACE") {
        level_ = Level::TRACE;
    } else if (logLevel == "DEBUG") {
        level_ = Level::DEBUG;
    } else {
        std::cerr << "Invalid log level: " << logLevel
                  << ". It should be one of [INFO|TRACE|DEBUG].\n";
    }
}

/**
 * @brief Copy constructor for the `Log` class.
 *
 * Copies the configuration and log level from an existing `Log` instance.
 *
 * @param log Shared pointer to the source `Log` object.
 */
Log::Log(const std::shared_ptr<Log> &log)
    : config_(log->config_), level_(log->level_) {}

/**
 * @brief Destructor for the `Log` class.
 *
 * Ensures cleanup of any resources, although none are dynamically allocated here.
 */
Log::~Log() {}

/**
 * @brief Writes a log message to the log file and console.
 *
 * Ensures thread safety using a mutex. Messages are logged only if their severity
 * level meets or exceeds the configured logging level, or if they are errors.
 *
 * @param message The log message to write.
 * @param level The severity level of the message.
 */
void Log::write(const std::string &message, Level level) const {
    std::lock_guard<std::mutex> lock(logMutex_);

    if (level <= level_ || level == Level::ERROR) {
        std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            auto now = std::time(nullptr);
            auto localTime = *std::localtime(&now);
            std::ostringstream timestampStream;
            timestampStream << std::put_time(&localTime, "%Y-%m-%d_%H:%M:%S");
            std::string timestamp = timestampStream.str();

            std::string line = timestamp + " [" + mode_ + "]" + " [" +
                               levelToString(level) + "] " + message + "\n";

            logFile << line;
            std::cout << line;
        } else {
            std::cerr << "Error opening log file: " << config_->log().file
                      << ". Make sure the directory and file exist.\n";
        }
    }
}

/**
 * @brief Converts a log level enum value to its string representation.
 *
 * @param level The log level to convert.
 * @return The string representation of the log level.
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
            return "UNKNOWN";
    }
}
