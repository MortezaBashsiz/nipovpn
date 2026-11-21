#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.hpp"
#include "log.hpp"

class Usage : private Uncopyable {
public:
    struct Counter {
        explicit Counter(std::string userName) : name(std::move(userName)) {}

        const std::string name;
        std::atomic<std::uint64_t> used{0};
        std::atomic<std::uint64_t> limit{0};
        std::atomic<bool> blocked{false};
        std::uint64_t lastWritten = 0;
    };
    using CounterPtr = std::shared_ptr<Counter>;

    using LimitHandler = std::function<void(const std::string &user)>;

    static Usage &instance();

    void configure(const Config &config, LimitHandler onLimit, Log &log);

    CounterPtr counter(const std::string &user);

    void add(const CounterPtr &counter, std::uint64_t bytes);

    static bool overLimit(const Counter &counter);

    void flush(Log &log);

    static std::string periodStart(int resetDay);

private:
    Usage() = default;

    std::vector<std::string> refreshBlocked(Log &log);

    std::mutex mutex_;
    std::unordered_map<std::string, CounterPtr> counters_;
    std::unordered_set<std::string> active_;
    LimitHandler onLimit_;
    bool enabled_ = false;
    std::string file_;
    int resetDay_ = 1;
    std::string period_;
    bool loaded_ = false;
};
