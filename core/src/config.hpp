#pragma once

#include <yaml-cpp/yaml.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include "general.hpp"

/**
 * @brief Defines the runtime mode of the application.
 *
 * @details
 * - `server`: Runs in server mode, accepting incoming client connections.
 * - `agent`: Runs in agent mode, forwarding traffic to a remote server.
 */
enum class RunMode {
    server,
    agent
};

/**
 * @brief The `Config` class manages application configuration loaded from a YAML file.
 *
 * @details
 * - Responsible for parsing and storing configuration values.
 * - Provides structured access to configuration sections:
 *   - General
 *   - Log
 *   - Server
 *   - Agent
 * - Ensures thread-safe access to mutable runtime parameters.
 * - Supports both server and agent runtime modes.
 *
 * @note
 * - Inherits from `Uncopyable` to prevent unintended copying.
 * - Uses shared ownership via `std::shared_ptr` (see `pointer` alias).
 * - Configuration values are primarily immutable after initialization,
 *   except for selected runtime-adjustable parameters.
 */
class Config : private Uncopyable {
private:
    /**
     * @brief General configuration section.
     *
     * @details
     * - Contains global settings shared across components.
     */
    struct General {
        std::string token;      ///< Shared secret used for encryption/decryption.
        std::string fakeUrl;    ///< Fake URL used for obfuscation.
        std::string method;     ///< HTTP method used for requests.
        unsigned short timeout; ///< Network timeout in seconds.
        std::string chunkHeader;///< Custom header for chunked communication.
    };

    /**
     * @brief Logging configuration section.
     */
    struct Log {
        std::string level;///< Log level (e.g., DEBUG, INFO, ERROR).
        std::string file; ///< Output file path for logs.
    };

    /**
     * @brief Server configuration section.
     *
     * @details
     * - Defines listening parameters and TLS settings for server mode.
     */
    struct Server {
        unsigned short threads;   ///< Number of worker threads.
        std::string listenIp;     ///< IP address to bind.
        unsigned short listenPort;///< Port to listen on.

        bool tlsEnable;         ///< Enables TLS for incoming connections.
        bool tlsVerifyPeer;     ///< Enables peer certificate verification.
        std::string tlsCertFile;///< Path to TLS certificate file.
        std::string tlsKeyFile; ///< Path to TLS private key file.
        std::string tlsCaFile;  ///< Path to CA certificate file.
    };

    /**
     * @brief Agent configuration section.
     *
     * @details
     * - Defines client-side behavior and remote server connection settings.
     */
    struct Agent {
        unsigned short threads;   ///< Number of worker threads.
        std::string listenIp;     ///< Local listening IP address.
        unsigned short listenPort;///< Local listening port.
        std::string serverIp;     ///< Remote server IP address.
        unsigned short serverPort;///< Remote server port.
        std::string httpVersion;  ///< HTTP version used for requests.
        std::string userAgent;    ///< User-Agent header value.

        bool tlsEnable;       ///< Enables TLS for outbound connections.
        bool tlsVerifyPeer;   ///< Enables server certificate verification.
        std::string tlsCaFile;///< Path to CA certificate file.
    };

    const RunMode &runMode_;     ///< Current runtime mode.
    std::string filePath_;       ///< Path to the configuration file.
    const YAML::Node configYaml_;///< Parsed YAML configuration root.

    unsigned short threads_;   ///< Runtime thread count.
    std::string listenIp_;     ///< Runtime listening IP.
    unsigned short listenPort_;///< Runtime listening port.

    mutable std::mutex configMutex_;///< Mutex for thread-safe runtime updates.

    /**
     * @brief Private constructor for initializing configuration from file.
     *
     * @param mode Runtime mode (server or agent).
     * @param filePath Path to the YAML configuration file.
     */
    explicit Config(const RunMode &mode, const std::string &filePath);

public:
    using pointer = std::shared_ptr<Config>;

    /**
     * @brief Factory method to create a new `Config` instance.
     *
     * @param mode Runtime mode.
     * @param filePath Path to the configuration file.
     * @return Shared pointer to the created `Config`.
     */
    static pointer create(const RunMode &mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    const General general_;///< General configuration section.
    const Log log_;        ///< Logging configuration section.
    const Server server_;  ///< Server configuration section.
    const Agent agent_;    ///< Agent configuration section.

    /**
     * @brief Copy constructor from shared pointer.
     *
     * @param config Existing configuration instance.
     */
    explicit Config(const Config::pointer &config);

    /**
     * @brief Destructor for `Config`.
     */
    ~Config();

    /// @name Section Accessors
    /// @{

    const General &general() const;
    const Log &log() const;
    const Server &server() const;
    const Agent &agent() const;

    /// @}

    /// @name Runtime Configuration Accessors
    /// @{

    /**
     * @brief Gets the current thread count.
     */
    const unsigned short &threads() const;

    /**
     * @brief Sets the thread count.
     */
    void threads(unsigned short threads);

    /**
     * @brief Gets the listening IP address.
     */
    const std::string &listenIp() const;

    /**
     * @brief Sets the listening IP address.
     */
    void listenIp(const std::string &ip);

    /**
     * @brief Gets the listening port.
     */
    const unsigned short &listenPort() const;

    /**
     * @brief Sets the listening port.
     */
    void listenPort(unsigned short port);

    /// @}

    /**
     * @brief Gets the current runtime mode.
     */
    const RunMode &runMode() const;

    /**
     * @brief Gets the configuration file path.
     */
    const std::string &filePath() const;

    /**
     * @brief Converts the runtime mode to a string representation.
     *
     * @return String representation of the mode.
     */
    std::string modeToString() const;

    /**
     * @brief Converts the full configuration into a human-readable string.
     *
     * @return Serialized configuration string.
     */
    std::string toString() const;
};