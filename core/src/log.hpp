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
 * @class Log
 * @brief Provides logging functionality with different log levels and thread safety.
 *
 * The `Log` class is responsible for writing log messages to a file or other output.
 * It supports various log levels (INFO, TRACE, ERROR, DEBUG) and ensures thread-safe
 * logging using a mutex.
 */
class Log : private Uncopyable {
public:
    /**
     * @enum Level
     * @brief Defines the severity levels of log messages.
     */
    enum class Level {
        INFO,
        TRACE,
        ERROR,
        DEBUG
    };

    using pointer = std::shared_ptr<Log>;

    /**
     * @brief Creates a new instance of the Log class.
     *
     * This static method is the primary way to instantiate a Log object. It ensures
     * proper encapsulation and memory management via `std::shared_ptr`.
     *
     * @param config Shared pointer to the configuration object.
     * @return A shared pointer to the newly created Log instance.
     */
    static pointer create(const std::shared_ptr<Config> &config) {
        return pointer(new Log(config));
    }

    /**
     * @brief Constructs a Log object by copying another Log instance.
     *
     * @param log Shared pointer to the Log object to copy.
     */
    explicit Log(const std::shared_ptr<Log> &log);

    /**
     * @brief Destructor for the Log class.
     *
     * Ensures proper cleanup of resources, such as closing log files, if necessary.
     */
    ~Log();

    /**
     * @brief Gets the configuration associated with this Log instance.
     *
     * @return A shared pointer to the configuration object.
     */
    const std::shared_ptr<Config> &config() const { return config_; }

    /**
     * @brief Gets the current logging level.
     *
     * @return The current logging level as an enum value.
     */
    Level level() const { return level_; }

    /**
     * @brief Writes a log message with a specified log level.
     *
     * This function is thread-safe and ensures that multiple threads can log messages
     * without interfering with each other.
     *
     * @param message The log message to write.
     * @param level The severity level of the message.
     */
    void write(const std::string &message, Level level) const;

private:
    /**
     * @brief Constructs a Log object with a specified configuration.
     *
     * This constructor is private to enforce the use of the `create` factory method.
     *
     * @param config Shared pointer to the configuration object.
     */
    explicit Log(const std::shared_ptr<Config> &config);

    /**
     * @brief Converts a log level enum value to a string representation.
     *
     * @param level The log level to convert.
     * @return A string representation of the log level.
     */
    static std::string levelToString(Level level);

    const std::shared_ptr<Config> &config_;
    Level level_;
    std::string mode_;

    mutable std::mutex logMutex_;
};
