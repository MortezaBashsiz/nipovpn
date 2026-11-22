#include "runner.hpp"

#include "usage.hpp"

Runner::Runner(const std::shared_ptr<Config> &config,
               const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(),
      work_guard_(boost::asio::make_work_guard(io_context_)),
      running_(true),
      strand_(boost::asio::make_strand(io_context_)),
      signals_(io_context_),
      stopSignals_(io_context_),
      usageTimer_(io_context_) {}

Runner::~Runner() {
    running_.store(false);
    io_context_.stop();

    for (auto &thread: threadPool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    ServerHandler::closeAllSessions();
}

void Runner::run() {
    try {
        log_->write("Config initialized in " + config_->modeToString() + " mode", Log::Level::INFO);
        log_->write(config_->toString(), Log::Level::INFO);

        warnAboutSampleToken();

        auto tcpServer = TCPServer::create(io_context_, config_, log_);

        waitForReloadSignal();
        waitForStopSignal();
        startUsage();

        for (auto i = 0; i < config_->threads(); ++i) {
            threadPool_.emplace_back([this] { workerThread(); });
        }

        for (auto &thread: threadPool_) {
            if (thread.joinable()) {
                thread.join();
            }
        }

    } catch (const std::exception &error) {
        log_->write(std::string("[Runner run] ") + error.what(), Log::Level::ERROR);
    } catch (...) {
        log_->write("[Runner run] Unknown error occurred", Log::Level::ERROR);
    }
}

void Runner::workerThread() {
    while (running_.load()) {
        try {
            io_context_.run();
            break;
        } catch (const std::exception &e) {
            log_->write(std::string("[WorkerThread] Exception: ") + e.what(), Log::Level::ERROR);
        } catch (...) {
            log_->write("[WorkerThread] Unknown error occurred", Log::Level::ERROR);
        }
    }
}

void Runner::stop() {
    running_.store(false);
    io_context_.stop();
}

void Runner::warnAboutSampleToken() {
    for (const auto &warning: config_->warnings()) {
        log_->write("[WARNING] " + warning, Log::Level::ERROR);
    }

    if (config_->runMode() == RunMode::agent) {
        if (config_->agent().token == SampleToken) {
            log_->write(
                    "[WARNING] agent.token is the public sample token; "
                    "set a unique random token for each user",
                    Log::Level::ERROR);
        }
        return;
    }

    for (const auto &user: config_->users()) {
        if (user.enabled && user.token == SampleToken) {
            log_->write("[WARNING] user " + user.name +
                                " uses the public sample token; "
                                "set a unique random token for each user",
                        Log::Level::ERROR);
        }
    }
}

void Runner::waitForReloadSignal() {
#if defined(SIGHUP)
    boost::system::error_code ec;
    signals_.add(SIGHUP, ec);
    if (ec) {
        log_->write("[Runner] cannot handle SIGHUP, reload disabled: " + ec.message(),
                    Log::Level::ERROR);
        return;
    }

    signals_.async_wait([this](const boost::system::error_code &error, int) {
        if (error) return;
        reload();
        waitForReloadSignal();
    });
#endif
}

void Runner::reload() {
    log_->write("[Runner reload] reloading " + config_->filePath(), Log::Level::INFO);

    const Config::ReloadResult result = config_->reload();
    if (!result.ok) {
        log_->write("[Runner reload] " + result.message, Log::Level::ERROR);
        return;
    }

    log_->applyConfigLevel();
    log_->write("[Runner reload] " + result.message, Log::Level::INFO);
    warnAboutSampleToken();

    if (config_->runMode() == RunMode::server) {
        Usage::instance().configure(*config_, nullptr, *log_);
        Usage::instance().flush(*log_);
    }

    if (!result.revokedUsers.empty()) {
        const std::size_t connections = TCPConnection::revokeUsers(result.revokedUsers);
        const std::size_t sessions = ServerHandler::revokeSessions(result.revokedUsers);
        log_->write("[Runner reload] closed " + std::to_string(connections) +
                            " connections and " + std::to_string(sessions) +
                            " tunnel sessions of revoked users",
                    Log::Level::INFO);
    }
}

void Runner::startUsage() {
    if (config_->runMode() != RunMode::server) return;

    Usage::instance().configure(
            *config_, [this](const std::string &user) { onUsageLimit(user); }, *log_);
    if (!config_->hasUserList()) return;

    Usage::instance().flush(*log_);
    log_->write("[Usage] tracking usage in " + config_->usageFile() + ", period from " +
                        Usage::periodStart(config_->usageResetDay()),
                Log::Level::INFO);
    scheduleUsageFlush();
}

void Runner::scheduleUsageFlush() {
    usageTimer_.expires_after(std::chrono::seconds(30));
    usageTimer_.async_wait([this](const boost::system::error_code &error) {
        if (error) return;
        Usage::instance().flush(*log_);
        scheduleUsageFlush();
    });
}

void Runner::onUsageLimit(const std::string &user) {
    std::string limit;
    for (const auto &u: config_->users()) {
        if (u.name == user) limit = formatBytes(u.limit);
    }
    log_->write("[Usage] user " + user + " reached their limit of " + limit +
                        "; blocked until the next period",
                Log::Level::INFO);
    TCPConnection::revokeUsers({user});
    ServerHandler::revokeSessions({user});
}

void Runner::waitForStopSignal() {
    boost::system::error_code ec;
    stopSignals_.add(SIGINT, ec);
    stopSignals_.add(SIGTERM, ec);

    stopSignals_.async_wait([this](const boost::system::error_code &error, int signal) {
        if (error) return;
        log_->write("[Runner] received signal " + std::to_string(signal) + ", stopping",
                    Log::Level::INFO);
        if (config_->runMode() == RunMode::server) {
            Usage::instance().flush(*log_);
        }
        stop();
    });
}
