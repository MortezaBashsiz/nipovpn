#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <yaml-cpp/yaml.h>

#include <memory>
#include <sstream>

#include "general.hpp"

/**
 * @brief Enum to define the operational modes of the application.
 * - `server`: Indicates that the application is running in server mode.
 * - `agent`: Indicates that the application is running in agent mode.
 */
enum class RunMode { server,
                     agent };

/**
 * @brief Class to handle configuration settings for the application.
 *
 * This class reads and stores configuration settings from a YAML file.
 * It provides access to different configuration parameters based on the
 * operational mode of the application (server or agent).
 */
class Config : private Uncopyable {
private:
    /**
   * @brief Struct to hold general configuration settings.
   */
    struct General {
        std::string fakeUrl;      ///< URL used for fake requests.
        std::string method;       ///< HTTP method (e.g., GET, POST).
        unsigned int timeWait;    ///< Time to wait before sending requests.
        unsigned short repeatWait;///< Time to wait between repeated requests.
    };

    /**
   * @brief Struct to hold logging configuration settings.
   */
    struct Log {
        std::string level;///< Log level (e.g., DEBUG, ERROR).
        std::string file; ///< File path for the log output.
    };

    /**
   * @brief Struct to hold server-specific configuration settings.
   */
    struct Server {
        unsigned short threads;   ///< Number of threads for the server.
        std::string listenIp;     ///< IP address for the server to listen on.
        unsigned short listenPort;///< Port number for the server to listen on.
    };

    /**
   * @brief Struct to hold agent-specific configuration settings.
   */
    struct Agent {
        unsigned short threads;   ///< Number of threads for the agent.
        std::string listenIp;     ///< IP address for the agent to listen on.
        unsigned short listenPort;///< Port number for the agent to listen on.
        std::string serverIp;     ///< IP address of the server the agent connects to.
        unsigned short
                serverPort;     ///< Port number of the server the agent connects to.
        std::string token;      ///< Authentication token for the agent.
        std::string httpVersion;///< HTTP version used by the agent.
        std::string userAgent;  ///< User-Agent string used by the agent.
    };

    const RunMode &runMode_;///< The operational mode of the application.
    std::string filePath_;  ///< Path to the configuration YAML file.
    const YAML::Node
            configYaml_;       ///< YAML node for parsing the configuration file.
    unsigned short threads_;   ///< Number of threads to use based on mode.
    std::string listenIp_;     ///< IP address to listen on, based on mode.
    unsigned short listenPort_;///< Port number to listen on, based on mode.

    /**
   * @brief Constructs a Config instance with the specified mode and file path.
   *
   * @param mode The operational mode (server or agent).
   * @param filePath The path to the configuration file.
   */
    explicit Config(const RunMode &mode, const std::string &filePath);

public:
    using pointer = std::shared_ptr<Config>;///< Shared pointer type for Config.

    /**
   * @brief Factory method to create a Config instance.
   *
   * @param mode The operational mode (server or agent).
   * @param filePath The path to the configuration file.
   * @return A shared pointer to the created Config instance.
   */
    static pointer create(const RunMode &mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    const General general_;///< General configuration settings.
    const Log log_;        ///< Logging configuration settings.
    const Server server_;  ///< Server-specific configuration settings.
    const Agent agent_;    ///< Agent-specific configuration settings.

    /**
   * @brief Copy constructor to create a Config instance from another Config
   * pointer.
   *
   * @param config A shared pointer to another Config instance.
   */
    explicit Config(const Config::pointer &config);

    /**
   * @brief Destructor for Config.
   */
    ~Config();

    /**
   * @brief Returns the general configuration settings.
   *
   * @return A constant reference to the General struct.
   */
    const General &general() const;

    /**
   * @brief Returns the logging configuration settings.
   *
   * @return A constant reference to the Log struct.
   */
    const Log &log() const;

    /**
   * @brief Returns the server-specific configuration settings.
   *
   * @return A constant reference to the Server struct.
   */
    const Server &server() const;

    /**
   * @brief Returns the agent-specific configuration settings.
   *
   * @return A constant reference to the Agent struct.
   */
    const Agent &agent() const;

    /**
   * @brief Returns the number of threads configured.
   *
   * @return A constant reference to the number of threads.
   */
    const unsigned short &threads() const;

    /**
   * @brief Sets the number of threads in the configuration.
   *
   * @param threads The number of threads to set.
   */
    void threads(unsigned short threads);

    /**
   * @brief Returns the IP address for listening.
   *
   * @return A constant reference to the IP address.
   */
    const std::string &listenIp() const;

    /**
   * @brief Sets the IP address for listening.
   *
   * @param ip The IP address to set.
   */
    void listenIp(const std::string &ip);

    /**
   * @brief Returns the port number for listening.
   *
   * @return A constant reference to the port number.
   */
    const unsigned short &listenPort() const;

    /**
   * @brief Sets the port number for listening.
   *
   * @param port The port number to set.
   */
    void listenPort(unsigned short port);

    /**
   * @brief Returns the current operational mode of the application.
   *
   * @return A constant reference to the operational mode.
   */
    const RunMode &runMode() const;

    /**
   * @brief Returns the path to the configuration file.
   *
   * @return A constant reference to the file path.
   */
    const std::string &filePath() const;

    /**
   * @brief Converts the operational mode to a string representation.
   *
   * @return A string describing the operational mode.
   */
    std::string modeToString() const;

    /**
   * @brief Returns a string representation of the entire configuration.
   *
   * @return A string describing all configuration settings.
   */
    std::string toString() const;
};

#endif /* CONFIG_HPP */
