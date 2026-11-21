#include "log.hpp"

Log::Log(const std::shared_ptr<Config> &config)
    : config_(config), level_(Level::INFO) {

    if (config_->runMode() == RunMode::server)
        mode_ = std::string("SERVER");
    else if (config_->runMode() == RunMode::agent)
        mode_ = std::string("AGENT");

    std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error opening log file: " << config_->log().file
                  << ". Make sure the directory and file exist.\n";
    }

    applyConfigLevel();
}

bool Log::parseLevel(const std::string &name, Level &out) {
    if (name == "INFO") {
        out = Level::INFO;
    } else if (name == "TRACE") {
        out = Level::TRACE;
    } else if (name == "DEBUG") {
        out = Level::DEBUG;
    } else {
        return false;
    }
    return true;
}

void Log::applyConfigLevel() {
    const std::string logLevel = config_->log().level;
    Level parsed;
    if (parseLevel(logLevel, parsed)) {
        level_.store(parsed);
    } else {
        std::cerr << "Invalid log level: " << logLevel
                  << ". It should be one of [INFO|TRACE|DEBUG].\n";
    }
}

Log::Log(const std::shared_ptr<Log> &log)
    : config_(log->config_), level_(log->level_.load()) {}

Log::~Log() {}

void Log::write(const std::string &message, Level level) const {
    std::lock_guard<std::mutex> lock(logMutex_);

    if (level <= level_.load() || level == Level::ERROR) {

        std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);

        if (logFile.is_open()) {

            auto now = std::time(nullptr);
            auto localTime = *std::localtime(&now);

            std::ostringstream timestampStream;
            timestampStream << std::put_time(&localTime, "%Y-%m-%d_%H:%M:%S");
            std::string timestamp = timestampStream.str();

            std::string line = timestamp + " [" + mode_ + "]" + " [" +
                               levelToString(level) + "] " + message + "\n";

            logFile << line;
            std::cout << line;

        } else {
            std::cerr << "Error opening log file: " << config_->log().file
                      << ". Make sure the directory and file exist.\n";
        }
    }
}

std::string Log::levelToString(Level level) {
    switch (level) {
        case Level::INFO:
            return "INFO";
        case Level::TRACE:
            return "TRACE";
        case Level::ERROR:
            return "ERROR";
        case Level::DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}