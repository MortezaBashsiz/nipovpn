#include "log.hpp"

/**
 * @brief Constructs a `Log` instance using the provided configuration.
 *
 * @details
 * - Initializes the runtime mode string based on the configured `RunMode`.
 * - Attempts to open the configured log file to verify accessibility.
 * - Initializes the logging level based on configuration:
 *   - Supported levels: INFO, TRACE, DEBUG
 *   - Defaults to INFO if not specified or invalid
 * - Prints an error to `std::cerr` if the log file cannot be opened or
 *   if an invalid log level is provided.
 *
 * @param config Shared pointer to the configuration object.
 */
Log::Log(const std::shared_ptr<Config> &config)
    : config_(config), level_(Level::INFO) {

    /**
     * @brief Determines the runtime mode string for log entries.
     */
    if (config_->runMode() == RunMode::server)
        mode_ = std::string("SERVER");
    else if (config_->runMode() == RunMode::agent)
        mode_ = std::string("AGENT");

    /**
     * @brief Verifies that the log file can be opened.
     */
    std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error opening log file: " << config_->log().file
                  << ". Make sure the directory and file exist.\n";
    }

    /**
     * @brief Sets the logging level based on configuration.
     */
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
 * @brief Copy-constructs a `Log` instance from another instance.
 *
 * @details
 * - Copies the configuration reference and logging level.
 * - Does not duplicate file handles or internal state beyond configuration.
 *
 * @param log Shared pointer to the source `Log` instance.
 */
Log::Log(const std::shared_ptr<Log> &log)
    : config_(log->config_), level_(log->level_) {}

/**
 * @brief Destroys the `Log` instance.
 *
 * @details
 * - No explicit resource cleanup is required.
 * - File streams are opened and closed per write operation.
 */
Log::~Log() {}

/**
 * @brief Writes a log message to the configured output.
 *
 * @details
 * - Ensures thread safety using an internal mutex.
 * - Writes messages only if:
 *   - The message level is less than or equal to the configured level, OR
 *   - The message level is `ERROR`.
 * - Each log entry includes:
 *   - Timestamp (YYYY-MM-DD_HH:MM:SS)
 *   - Runtime mode (SERVER or AGENT)
 *   - Log level
 *   - Message content
 * - Outputs the log entry to both:
 *   - The configured log file
 *   - Standard output (`std::cout`)
 * - Prints an error to `std::cerr` if the log file cannot be opened.
 *
 * @param message Log message to write.
 * @param level Severity level of the message.
 */
void Log::write(const std::string &message, Level level) const {
    std::lock_guard<std::mutex> lock(logMutex_);

    if (level <= level_ || level == Level::ERROR) {

        std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);

        if (logFile.is_open()) {

            /**
             * @brief Generates a formatted timestamp.
             */
            auto now = std::time(nullptr);
            auto localTime = *std::localtime(&now);

            std::ostringstream timestampStream;
            timestampStream << std::put_time(&localTime, "%Y-%m-%d_%H:%M:%S");
            std::string timestamp = timestampStream.str();

            /**
             * @brief Formats the final log line.
             */
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
 * @details
 * - Maps each `Level` enum value to its corresponding string.
 * - Returns `"UNKNOWN"` for unsupported values.
 *
 * @param level Log level to convert.
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
            return "UNKNOWN";
    }
}