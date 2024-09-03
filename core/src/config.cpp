#include "config.hpp"

/**
 * @brief Constructs a Config instance with specified mode and file path.
 *
 * @param mode The operational mode of the application (server or agent).
 * @param filePath The path to the configuration file.
 */
Config::Config(const RunMode &mode, const std::string &filePath)
    : runMode_(mode),
      filePath_(filePath),
      configYaml_(YAML::LoadFile(filePath)),
      threads_(0),
      listenIp_("127.0.0.1"),
      listenPort_(0),
      general_({configYaml_["general"]["fakeUrl"].as<std::string>(),
                configYaml_["general"]["method"].as<std::string>(),
                configYaml_["general"]["timeWait"].as<unsigned int>(),
                configYaml_["general"]["repeatWait"].as<unsigned short>()}),
      log_({configYaml_["log"]["logLevel"].as<std::string>(),
            configYaml_["log"]["logFile"].as<std::string>()}),
      server_({configYaml_["server"]["threads"].as<unsigned short>(),
               configYaml_["server"]["listenIp"].as<std::string>(),
               configYaml_["server"]["listenPort"].as<unsigned short>()}),
      agent_({configYaml_["agent"]["threads"].as<unsigned short>(),
              configYaml_["agent"]["listenIp"].as<std::string>(),
              configYaml_["agent"]["listenPort"].as<unsigned short>(),
              configYaml_["agent"]["serverIp"].as<std::string>(),
              configYaml_["agent"]["serverPort"].as<unsigned short>(),
              configYaml_["agent"]["token"].as<std::string>(),
              configYaml_["agent"]["httpVersion"].as<std::string>(),
              configYaml_["agent"]["userAgent"].as<std::string>()}) {
    std::lock_guard<std::mutex> lock(configMutex_);
    switch (runMode_) {
        case RunMode::server:
            threads_ = server_.threads;
            listenIp_ = server_.listenIp;
            listenPort_ = server_.listenPort;
            break;
        case RunMode::agent:
            threads_ = agent_.threads;
            listenIp_ = agent_.listenIp;
            listenPort_ = agent_.listenPort;
            break;
    }
}

/**
 * @brief Copy constructor for Config.
 *
 * @param config A shared pointer to another Config instance.
 */
Config::Config(const Config::pointer &config)
    : runMode_(config->runMode()),
      configYaml_(YAML::LoadFile(config->filePath())),
      general_(config->general()),
      log_(config->log()),
      server_(config->server()),
      agent_(config->agent()) {}

/**
 * @brief Destructor for Config.
 */
Config::~Config() = default;

/**
 * @brief Returns a string representation of the current configuration.
 *
 * @return A string describing the configuration settings.
 */
std::string Config::toString() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    std::stringstream ss;
    ss << "\nConfig :\n"
       << " General :\n"
       << "   fakeUrl: " << general_.fakeUrl << "\n"
       << "   method: " << general_.method << "\n"
       << "   timeWait: " << general_.timeWait << "\n"
       << "   repeatWait: " << general_.repeatWait << "\n"
       << " Log :\n"
       << "   logLevel: " << log_.level << "\n"
       << "   logFile: " << log_.file << "\n"
       << " server :\n"
       << "   threads: " << server_.threads << "\n"
       << "   listenIp: " << server_.listenIp << "\n"
       << "   listenPort: " << server_.listenPort << "\n"
       << " agent :\n"
       << "   threads: " << agent_.threads << "\n"
       << "   listenIp: " << agent_.listenIp << "\n"
       << "   listenPort: " << agent_.listenPort << "\n"
       << "   serverIp: " << agent_.serverIp << "\n"
       << "   serverPort: " << agent_.serverPort << "\n"
       << "   token: " << agent_.token << "\n"
       << "   httpVersion: " << agent_.httpVersion << "\n"
       << "   userAgent: " << agent_.userAgent << "\n";
    return ss.str();
}

const Config::General &Config::general() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return general_;
}

const Config::Log &Config::log() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return log_;
}

const Config::Server &Config::server() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return server_;
}

const Config::Agent &Config::agent() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return agent_;
}

const unsigned short &Config::threads() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return threads_;
}

void Config::threads(unsigned short threads) {
    std::lock_guard<std::mutex> lock(configMutex_);
    threads_ = threads;
}

const std::string &Config::listenIp() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenIp_;
}

void Config::listenIp(const std::string &ip) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenIp_ = ip;
}

const unsigned short &Config::listenPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenPort_;
}

void Config::listenPort(unsigned short port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenPort_ = port;
}

const RunMode &Config::runMode() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return runMode_;
}

const std::string &Config::filePath() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return filePath_;
}

std::string Config::modeToString() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    switch (runMode_) {
        case RunMode::server:
            return "server";
        case RunMode::agent:
            return "agent";
        default:
            return "UNKNOWN MODE";
    }
}
