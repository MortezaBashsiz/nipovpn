#pragma once

#include <atomic>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <string>

#include "config.hpp"
#include "general.hpp"

class Log : private Uncopyable {
public:
    enum class Level {
        INFO,
        TRACE,
        ERROR,
        DEBUG
    };

    using pointer = std::shared_ptr<Log>;

    static pointer create(const std::shared_ptr<Config> &config) {
        return pointer(new Log(config));
    }

    explicit Log(const std::shared_ptr<Log> &log);

    ~Log();

    std::shared_ptr<Config> config() const { return config_; }

    Level level() const { return level_.load(); }

    void applyConfigLevel();

    void write(const std::string &message, Level level) const;

private:
    explicit Log(const std::shared_ptr<Config> &config);

    static std::string levelToString(Level level);

    static bool parseLevel(const std::string &name, Level &out);

    std::shared_ptr<Config> config_;
    std::atomic<Level> level_;
    std::string mode_;

    mutable std::mutex logMutex_;
};