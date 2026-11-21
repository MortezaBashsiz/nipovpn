#include "usage.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

Usage &Usage::instance() {
    static Usage usage;
    return usage;
}

std::string Usage::periodStart(int resetDay) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    int year = local.tm_year + 1900;
    int month = local.tm_mon + 1;
    if (local.tm_mday < resetDay) {
        if (--month == 0) {
            month = 12;
            --year;
        }
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, resetDay);
    return buf;
}

bool Usage::overLimit(const Counter &counter) {
    const std::uint64_t limit = counter.limit.load();
    return limit != 0 && counter.used.load() >= limit;
}

void Usage::configure(const Config &config, LimitHandler onLimit, Log &log) {
    std::vector<std::string> newlyBlocked;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!onLimit_) onLimit_ = std::move(onLimit);

        enabled_ = config.runMode() == RunMode::server && config.hasUserList();
        if (!enabled_) return;

        file_ = config.usageFile();
        resetDay_ = config.usageResetDay();

        active_.clear();
        for (const auto &user: config.users()) {
            auto &counter = counters_[user.name];
            if (!counter) counter = std::make_shared<Counter>(user.name);
            counter->limit.store(user.limit);
            active_.insert(user.name);
        }
        newlyBlocked = refreshBlocked(log);
    }
    for (const auto &name: newlyBlocked) onLimit_(name);
}

Usage::CounterPtr Usage::counter(const std::string &user) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) return nullptr;
    auto it = counters_.find(user);
    return it == counters_.end() ? nullptr : it->second;
}

void Usage::add(const CounterPtr &counter, std::uint64_t bytes) {
    if (!counter || bytes == 0) return;

    const std::uint64_t after = counter->used.fetch_add(bytes) + bytes;
    const std::uint64_t limit = counter->limit.load();

    if (limit != 0 && after >= limit && !counter->blocked.exchange(true) && onLimit_) {
        onLimit_(counter->name);
    }
}

std::vector<std::string> Usage::refreshBlocked(Log &log) {
    std::vector<std::string> newlyBlocked;
    for (const auto &[name, counter]: counters_) {
        const bool over = overLimit(*counter);
        if (over && !counter->blocked.exchange(true)) {
            newlyBlocked.push_back(name);
        } else if (!over && counter->blocked.exchange(false)) {
            log.write("[Usage] user " + name + " is below their limit again", Log::Level::INFO);
        }
    }
    return newlyBlocked;
}

void Usage::flush(Log &log) {
    std::vector<std::string> newlyBlocked;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;

        YAML::Node doc;
        bool haveFile = false;
        if (fileExists(file_)) {
            try {
                doc = YAML::LoadFile(file_);
                haveFile = true;
            } catch (const std::exception &e) {
                log.write("[Usage] cannot read " + file_ + ": " + e.what() +
                                  "; usage is not saved until the file is fixed",
                          Log::Level::ERROR);
                return;
            }
        }

        const std::string filePeriod = haveFile ? doc["period"].as<std::string>("") : "";
        const bool firstFlush = !loaded_;
        if (firstFlush) {
            period_ = filePeriod.empty() ? periodStart(resetDay_) : filePeriod;
            loaded_ = true;
        }

        const YAML::Node fileUsers = haveFile ? doc["users"] : YAML::Node();
        if (haveFile && filePeriod == period_ && fileUsers.IsMap()) {
            for (auto &[name, counter]: counters_) {
                const YAML::Node entry = fileUsers[name];
                if (!entry || !entry["used"]) continue;

                std::uint64_t value = 0;
                try {
                    value = entry["used"].as<std::uint64_t>();
                } catch (const std::exception &) {
                    log.write("[Usage] used of " + name + " in " + file_ +
                                      " is not a number of bytes; ignored",
                              Log::Level::ERROR);
                    continue;
                }
                if (value == counter->lastWritten) continue;

                counter->used.fetch_add(value - counter->lastWritten);
                counter->lastWritten = value;
                if (!firstFlush) {
                    log.write("[Usage] usage of " + name + " set to " + formatBytes(value) +
                                      " from " + file_,
                              Log::Level::INFO);
                }
            }
        }

        const std::string current = periodStart(resetDay_);
        if (current != period_) {
            for (auto &[name, counter]: counters_) counter->used.store(0);
            log.write("[Usage] new usage period from " + current + "; usage of all users reset",
                      Log::Level::INFO);
            period_ = current;
        }

        newlyBlocked = refreshBlocked(log);

        std::vector<std::string> names;
        for (const auto &[name, counter]: counters_) names.push_back(name);
        std::sort(names.begin(), names.end());

        std::ostringstream out;
        out << "# NipoVPN usage: bytes each user sent and received since " << period_ << ".\n"
            << "# To change a user's usage, edit their \"used\" (for example set it to 0).\n"
            << "# The server applies the change within 30 seconds. Do not edit \"period\".\n"
            << "period: \"" << period_ << "\"\n"
            << "users:\n";

        std::vector<std::pair<Counter *, std::uint64_t>> written;
        for (const auto &name: names) {
            Counter &counter = *counters_[name];
            const std::uint64_t used = counter.used.load();
            const std::uint64_t limit = counter.limit.load();
            out << "  " << name << ": {used: " << used << "}  # " << formatBytes(used);
            if (!active_.count(name)) {
                out << ", not in the users list";
            } else if (limit != 0) {
                out << " of " << formatBytes(limit) << " ("
                    << std::min<std::uint64_t>(used * 100 / limit, 999) << "%)"
                    << (used >= limit ? ", blocked" : "");
            } else {
                out << ", no limit";
            }
            out << "\n";
            written.emplace_back(&counter, used);
        }

        if (haveFile && filePeriod == period_ && fileUsers.IsMap()) {
            for (const auto &entry: fileUsers) {
                const std::string name = entry.first.as<std::string>("");
                if (name.empty() || counters_.count(name) || !isValidUserName(name)) continue;
                out << "  " << name << ": {used: " << entry.second["used"].as<std::uint64_t>(0)
                    << "}  # not in the users list\n";
            }
        }

        try {
            const std::filesystem::path path(file_);
            if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
            const std::string tmp = file_ + ".tmp";
            {
                std::ofstream f(tmp, std::ios::out | std::ios::trunc);
                if (!f) throw std::runtime_error("cannot open " + tmp + " for writing");
                f << out.str();
                f.flush();
                if (!f) throw std::runtime_error("cannot write " + tmp);
            }
            std::filesystem::rename(tmp, path);
            for (const auto &[counter, used]: written) counter->lastWritten = used;
        } catch (const std::exception &e) {
            log.write(std::string("[Usage] cannot save usage: ") + e.what(), Log::Level::ERROR);
        }
    }
    for (const auto &name: newlyBlocked) onLimit_(name);
}
