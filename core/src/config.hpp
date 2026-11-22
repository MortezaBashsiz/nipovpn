#pragma once

#include <yaml-cpp/yaml.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "general.hpp"

enum class RunMode {
    server,
    agent
};

struct User {
    std::string name;
    std::string token;
    std::string key;
    bool enabled;
    std::uint64_t limit = 0;
};

class Config : private Uncopyable {
public:
    struct General {
        unsigned short timeout;
        bool tunnelEnable;
        bool aesEnable;
        bool tlsEnable;
    };

    struct Log {
        std::string level;
        std::string file;
    };

    struct Server {
        unsigned short threads;
        std::string listenIp;
        unsigned short listenPort;
        unsigned int bufferSize;
        std::string tlsCertFile;
        std::string tlsKeyFile;
    };

    struct Agent {
        unsigned short threads;
        std::string listenIp;
        unsigned short listenPort;
        std::string serverIp;
        unsigned short serverPort;
        std::string httpVersion;
        std::string userAgent;
        std::string token;
        std::string protocol;
        std::vector<std::string> fakeUrls;
        std::vector<std::string> methods;
        std::vector<std::string> endPoints;
        unsigned short pullTimeout;
        bool connectionReuse;
        bool tlsVerifyPeer;
        std::string tlsCaFile;
    };

    struct Settings {
        General general;
        Log log;
        Server server;
        Agent agent;
        std::vector<User> users;
        bool usersFromFile = false;
        std::string usageFile;
        int usageResetDay = 1;
        std::vector<std::string> warnings;
    };

    struct ReloadResult {
        bool ok;
        std::string message;
        std::vector<std::string> revokedUsers;
    };

    using pointer = std::shared_ptr<Config>;

    static pointer create(RunMode mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    ~Config();

    const General &general() const;
    const Log &log() const;
    const Server &server() const;
    const Agent &agent() const;
    const std::vector<User> &users() const;
    const std::vector<std::string> &warnings() const;
    bool hasUserList() const;
    const std::string &usageFile() const;
    int usageResetDay() const;

    ReloadResult reload();

    unsigned short threads() const;

    void threads(unsigned short threads);

    const std::string &listenIp() const;

    void listenIp(const std::string &ip);

    unsigned short listenPort() const;

    void listenPort(unsigned short port);

    RunMode runMode() const;

    const std::string &filePath() const;

    std::string modeToString() const;

    std::string toString() const;

    std::string randomFakeUrl() const;

    std::string randomMethod() const;

    std::string randomEndPoint() const;

private:
    explicit Config(RunMode mode, const std::string &filePath);

    static std::unique_ptr<Settings> load(RunMode mode, const std::string &filePath);

    const Settings &current() const;

    void publish(std::unique_ptr<Settings> settings);

    RunMode runMode_;
    std::string filePath_;

    unsigned short threads_;
    std::string listenIp_;
    unsigned short listenPort_;

    mutable std::mutex configMutex_;

    std::atomic<const Settings *> current_{nullptr};
    std::vector<std::unique_ptr<const Settings>> snapshots_;
    std::mutex reloadMutex_;
};
