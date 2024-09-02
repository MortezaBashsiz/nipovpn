#include "log.hpp"

Log::Log(const std::shared_ptr<Config> &config)
    : config_(config), level_(Level::INFO) {
    if (config_->runMode() == RunMode::server)
        mode_ = std::string("SERVER");
    else if (config_->runMode() == RunMode::agent)
        mode_ = std::string("AGENT");

    // Open the log file for appending.
    std::ofstream logFile(config_->log().file, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error opening log file: " << config_->log().file
                  << ". Make sure the directory and file exist.\n";
    }

    const auto &logLevel = config_->log().level;
    if (logLevel == "INFO") {
        level_ = Level::INFO;
    } else if (logLevel == "TRACE") {
        level_ = Level::TRACE;
    } else if (logLevel == "DEBUG") {
        level_ = Level::DEBUG;
    } else {
        std::cerr << "Invalid log level: " << logLevel
                  << ". It should be one of [INFO|TRACE|DEBUG].\n";
    }
}

Log::Log(const std::shared_ptr<Log> &log)
    : config_(log->config_), level_(log->level_) {}

Log::~Log() {}

void Log::write(const std::string &message, Level level) const {
    std::lock_guard<std::mutex> lock(logMutex_);// Lock the mutex before writing

    if (level <= level_ || level == Level::ERROR) {
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
