#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <yaml-cpp/yaml.h>

#include <memory>
#include <mutex>// Include mutex header for thread safety
#include <sstream>

#include "general.hpp"

enum class RunMode { server,
                     agent };

class Config : private Uncopyable {
private:
    struct General {
        std::string fakeUrl;
        std::string method;
        unsigned int timeWait;
        unsigned short timeout;
        unsigned short repeatWait;
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
        std::string token;
        std::string httpVersion;
        std::string userAgent;
    };

    const RunMode &runMode_;
    std::string filePath_;
    const YAML::Node configYaml_;
    unsigned short threads_;
    std::string listenIp_;
    unsigned short listenPort_;

    mutable std::mutex configMutex_;// Mutex for thread safety

    explicit Config(const RunMode &mode, const std::string &filePath);

public:
    using pointer = std::shared_ptr<Config>;

    static pointer create(const RunMode &mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    const General general_;
    const Log log_;
    const Server server_;
    const Agent agent_;

    explicit Config(const Config::pointer &config);
    ~Config();

    const General &general() const;
    const Log &log() const;
    const Server &server() const;
    const Agent &agent() const;
    const unsigned short &threads() const;
    void threads(unsigned short threads);
    const std::string &listenIp() const;
    void listenIp(const std::string &ip);
    const unsigned short &listenPort() const;
    void listenPort(unsigned short port);
    const RunMode &runMode() const;
    const std::string &filePath() const;
    std::string modeToString() const;
    std::string toString() const;
};

#endif /* CONFIG_HPP */
