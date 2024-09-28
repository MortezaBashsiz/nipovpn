#pragma once

#include <yaml-cpp/yaml.h>

#include <memory>
#include <mutex>
#include <sstream>

#include "general.hpp"

enum class RunMode : std::uint8_t {
    server = 0,
    agent
};

class Config : private Uncopyable {
    /// TODO: Use PImpl Idiom to hide the structs from class interface.
    struct General {
        std::string fakeUrl;
        std::string method;
        std::uint32_t timeWait;
        std::uint16_t timeout;
        std::uint16_t repeatWait;
    };

    struct Log {
        std::string level;
        std::string file;
    };

    struct Server {
        std::uint16_t threads;
        std::uint16_t listenPort;
        std::string listenIp;
    };

    struct Agent {
        std::uint16_t threads;
        std::uint16_t listenPort;
        std::uint16_t serverPort;
        std::string listenIp;
        std::string serverIp;
        std::string token;
        std::string httpVersion;
        std::string userAgent;
    };

public:
    /// TODO: Make a wrapper around std::shared_ptr and boost::shared_ptr to abstract them.
    using pointer = std::shared_ptr<Config>;

    explicit Config(const Config::pointer &config);
    ~Config();

    /**
     * @brief Factory method to create a new Config instance.
     *
     * @param mode Program's running mode (client or server).
     * @param filePath Config file path.
     * @return Shared pointer to a new Config instance.
     */
    static pointer create(const RunMode &mode, const std::string &filePath);

    /**
     * @brief Accessor to the general configuration section of the config file.
     * @return Const-reference to the general configurations instance.
     */
    const General& getGeneralConfigs() const;

    /**
     * @brief Accessor to the log configuration section of the config file.
     * @return Const-reference to the log configurations instance.
     */
    const Log& getLogConfigs() const;

    /**
     * @brief Accessor to the server configuration section of the config file.
     * @return Const-reference to the server configurations instance.
     */
    const Server& getServerConfigs() const;

    /**
     * @brief Accessor to the agent configuration section of the config file.
     * @return Const-reference to the agent configurations instance.
     */
    const Agent& getAgentConfigs() const;

    /**
     * @brief Setter for threads count configuration.
     */
    void setThreadsCount(std::uint16_t threads);

    /**
     * @brief Getter for threads count configuration.
     * @return Number of threads.
     */
    std::uint16_t getThreadsCount() const;

    /**
     * @brief Setter for listen ip configuration.
     */
    void setListenIp(const std::string &ip);

    /**
     * @brief Getter for listen ip configuration.
     * @return Const-reference to Listen ip.
     */
    const std::string& getListenIp() const;

    /**
     * @brief Setter for listen port configuration.
     */
    void setListenPort(std::uint16_t port);

    /**
     * @brief Getter for listen port configuration.
     * @return Const-reference to Listen port.
     */
    std::uint16_t getListenPort() const;

    /**
     * @brief Getter for run-mode configuration.
     * @return Run-mode.
     */
    RunMode getRunMode() const;

    /**
     * @brief Get run-mode in string format instead of int.
     * @return String of run-mode
     */
    std::string getRunModeString() const;

    /**
     * @brief Getter for configuration file path.
     * @return Const-reference configuration file path.
     */
    const std::string &getFilePath() const;

    /**
     * @brief Create a string that shows all configuration sections all together.
     * @return The created string.
     */
    std::string getAllConfigsInStr() const;

private:
    /**
     * @brief Initializes the Config object.
     *
     * @param mode Program's running mode (client or server).
     * @param filePath Config file path.
     */
    explicit Config(const RunMode &mode, const std::string &filePath);

private:
    RunMode runMode_;
    std::string filePath_;
    YAML::Node configYaml_;
    std::uint16_t threadsCount_;
    std::string listenIp_;
    std::uint16_t listenPort_;

    General general_;
    const Log log_;
    const Server server_;
    const Agent agent_;

    mutable std::mutex configMutex_;
};
