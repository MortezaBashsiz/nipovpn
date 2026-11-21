#pragma once

#include <yaml-cpp/yaml.h>

#include <atomic>
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

// A server-side user. Users are told apart by their token: the server tries
// each enabled user's key until one authenticates the request.
struct User {
    std::string name;
    std::string token;
    std::string key;// deriveAesKey(token), computed once at load time
    bool enabled;
};

class Config : private Uncopyable {
public:
    struct General {
        std::string protocol;
        unsigned int bufferSize;
        std::vector<std::string> fakeUrls;
        std::vector<std::string> methods;
        std::vector<std::string> endPoints;
        unsigned short timeout;
        unsigned short pullTimeout;
        bool tunnelEnable;
        bool connectionReuse;
        bool aesEnable;
        bool tlsEnable;
        bool tlsVerifyPeer;
        std::string tlsCertFile;
        std::string tlsKeyFile;
        std::string tlsCaFile;
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
        // The token this agent sends. On the server it must belong to an
        // enabled user.
        std::string token;
    };

    // Everything read from the config file. A snapshot is never modified
    // after it is published.
    struct Settings {
        General general;
        Log log;
        Server server;
        Agent agent;
        // Server mode only. Without a users list in the file this holds one
        // user named "default" with the deprecated general.token.
        std::vector<User> users;
        bool usersFromFile = false;
        // Deprecated settings found in the file, to be logged.
        std::vector<std::string> warnings;
    };

    struct ReloadResult {
        bool ok;
        std::string message;
        // Users whose access ended: removed, disabled, or given a new token.
        std::vector<std::string> revokedUsers;
    };

    using pointer = std::shared_ptr<Config>;

    static pointer create(RunMode mode, const std::string &filePath) {
        return pointer(new Config(mode, filePath));
    }

    ~Config();

    // These return references into the current snapshot. Snapshots are kept
    // for the life of the process, so the references stay valid after a
    // reload; they just stop being current.
    const General &general() const;
    const Log &log() const;
    const Server &server() const;
    const Agent &agent() const;
    const std::vector<User> &users() const;
    const std::vector<std::string> &warnings() const;

    // Re-reads the config file. On any error the running config is kept.
    // listenIp, listenPort, threads and tlsEnable only change on restart.
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
