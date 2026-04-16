#pragma once

// Standard library includes
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <string>

// Project-specific includes
#include "config.hpp"
#include "general.hpp"

/**
 * @brief The `Log` class provides thread-safe logging functionality for the application.
 *
 * @details
 * - Supports multiple log severity levels.
 * - Writes log messages using configuration-defined logging behavior.
 * - Ensures thread-safe access through an internal mutex.
 * - Uses shared ownership via `std::shared_ptr`.
 *
 * @note
 * - Inherits from `Uncopyable` to prevent accidental copying.
 * - Instances should be created using the `create` factory method.
 */
class Log : private Uncopyable {
public:
    /**
     * @brief Enumeration of supported log severity levels.
     */
    enum class Level {
        INFO, ///< Informational messages.
        TRACE,///< Detailed tracing messages.
        ERROR,///< Error messages.
        DEBUG ///< Debugging messages.
    };

    using pointer = std::shared_ptr<Log>;

    /**
     * @brief Factory method to create a new `Log` instance.
     *
     * @details
     * - Wraps object creation in a `std::shared_ptr`.
     * - Ensures consistent construction and ownership handling.
     *
     * @param config Shared pointer to the configuration object.
     * @return A shared pointer to the created `Log` instance.
     */
    static pointer create(const std::shared_ptr<Config> &config) {
        return pointer(new Log(config));
    }

    /**
     * @brief Copy constructor that creates a `Log` instance from another `Log` object.
     *
     * @param log Shared pointer to the existing `Log` object.
     */
    explicit Log(const std::shared_ptr<Log> &log);

    /**
     * @brief Destructor for `Log`.
     *
     * @details
     * - Performs standard object cleanup.
     * - Any file handling or output cleanup is managed by implementation details.
     */
    ~Log();

    /**
     * @brief Returns the configuration associated with this logger.
     *
     * @return Const reference to the shared configuration pointer.
     */
    const std::shared_ptr<Config> &config() const { return config_; }

    /**
     * @brief Returns the currently configured log level.
     *
     * @return Current log severity level.
     */
    Level level() const { return level_; }

    /**
     * @brief Writes a log message using the specified severity level.
     *
     * @details
     * - Thread-safe through internal mutex protection.
     * - Uses configuration-defined logging mode and output destination.
     *
     * @param message Log message to write.
     * @param level Severity level of the message.
     */
    void write(const std::string &message, Level level) const;

private:
    /**
     * @brief Private constructor for creating a `Log` instance from configuration.
     *
     * @details
     * - Intended to be used only through the `create` factory method.
     *
     * @param config Shared pointer to the configuration object.
     */
    explicit Log(const std::shared_ptr<Config> &config);

    /**
     * @brief Converts a log level enum value to its string representation.
     *
     * @param level Log level to convert.
     * @return String representation of the log level.
     */
    static std::string levelToString(Level level);

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    Level level_;                          ///< Active logging level.
    std::string mode_;                     ///< Current runtime mode string.

    mutable std::mutex logMutex_;///< Mutex used to ensure thread-safe logging.
};