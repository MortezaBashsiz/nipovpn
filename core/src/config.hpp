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

    static pointer create(const RunMode &mode, const std::string &filePath);

    explicit Config(const Config::pointer &config);
    ~Config();

    const General& getGeneralConfigs() const;

    const Log& getLogConfigs() const;

    const Server& getServerConfigs() const;

    const Agent& getAgentConfigs() const;

    void setThreadsCount(std::uint16_t threads);
    std::uint16_t getThreadsCount() const;

    void setListenIp(const std::string &ip);
    const std::string& getListenIp() const;

    void setListenPort(std::uint16_t port);
    std::uint16_t getListenPort() const;

    RunMode getRunMode() const;
    std::string getRunModeString() const;

    const std::string &getFilePath() const;

    std::string getAllConfigsInStr() const;

private:    
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
