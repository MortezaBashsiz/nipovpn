#pragma once

#include <yaml-cpp/yaml.h>

#include <memory>
#include <mutex>
#include <sstream>

#include "general.hpp"

enum class RunMode { server,
                     agent };

class Config : private Uncopyable {
private:
    struct General {
        std::string token;
        std::string fakeUrl;
        std::string method;
        unsigned int timeWait;
        unsigned short timeout;
        unsigned short repeatWait;
        std::string chunkHeader;
    };

    struct Log {
        std::string level;
        std::string file;
    };

    struct Server {
        unsigned short threads;
        std::string listenIp;
        unsigned short listenPort;
    };

    struct Agent {
        unsigned short threads;
        std::string listenIp;
        unsigned short listenPort;
        std::string serverIp;
        unsigned short serverPort;
        std::string httpVersion;
        std::string userAgent;
    };

    const RunMode &runMode_;
    std::string filePath_;
    const YAML::Node configYaml_;
    unsigned short threads_;
    std::string listenIp_;
    unsigned short listenPort_;

    mutable std::mutex configMutex_;

    explicit Config(const RunMode &mode, const std::string &filePath);

public:
    using pointer = std::shared_ptr<Config>;

    /**
     * @brief Creates a shared pointer to a Config object.
     * 
     * @param mode The run mode (server or agent).
     * @param filePath Path to the configuration file.
     * @return pointer A shared pointer to the created Config instance.
     */
    static pointer create(const RunMode &mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    const General general_;
    const Log log_;
    const Server server_;
    const Agent agent_;

    explicit Config(const Config::pointer &config);
    ~Config();

    /**
     * @brief Retrieves the general configuration.
     * 
     * @return const General& A reference to the general configuration.
     */
    const General &general() const;

    /**
     * @brief Retrieves the logging configuration.
     * 
     * @return const Log& A reference to the logging configuration.
     */
    const Log &log() const;

    /**
     * @brief Retrieves the server configuration.
     * 
     * @return const Server& A reference to the server configuration.
     */
    const Server &server() const;

    /**
     * @brief Retrieves the agent configuration.
     * 
     * @return const Agent& A reference to the agent configuration.
     */
    const Agent &agent() const;

    /**
     * @brief Retrieves the number of threads configured.
     * 
     * @return const unsigned short& The number of threads.
     */
    const unsigned short &threads() const;

    /**
     * @brief Sets the number of threads in the configuration.
     * 
     * @param threads The number of threads to set.
     */
    void threads(unsigned short threads);

    /**
     * @brief Retrieves the IP address the application listens on.
     * 
     * @return const std::string& The listen IP address.
     */
    const std::string &listenIp() const;

    /**
     * @brief Sets the listen IP address in the configuration.
     * 
     * @param ip The IP address to set.
     */
    void listenIp(const std::string &ip);

    /**
     * @brief Retrieves the port number the application listens on.
     * 
     * @return const unsigned short& The listen port.
     */
    const unsigned short &listenPort() const;

    /**
     * @brief Sets the listen port number in the configuration.
     * 
     * @param port The port number to set.
     */
    void listenPort(unsigned short port);

    /**
     * @brief Retrieves the current run mode (server or agent).
     * 
     * @return const RunMode& The current run mode.
     */
    const RunMode &runMode() const;

    /**
     * @brief Retrieves the path to the configuration file.
     * 
     * @return const std::string& The configuration file path.
     */
    const std::string &filePath() const;

    /**
     * @brief Converts the current run mode to a string representation.
     * 
     * @return std::string A string representing the current run mode.
     */
    std::string modeToString() const;

    /**
     * @brief Converts the entire configuration to a string representation.
     * 
     * @return std::string A string representing the entire configuration.
     */
    std::string toString() const;
};
