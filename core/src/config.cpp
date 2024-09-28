#include "config.hpp"

Config::pointer Config::create(const RunMode& mode, const std::string& filePath)
{
    return pointer(new Config(mode, filePath));
}

Config::Config(const RunMode &mode, const std::string &filePath)
    : runMode_(mode),
      filePath_(filePath),
      configYaml_(YAML::LoadFile(filePath)),
      threadsCount_(0),
      listenIp_("127.0.0.1"),
      listenPort_(0),
      general_{
        configYaml_["general"]["fakeUrl"].as<std::string>(),
        configYaml_["general"]["method"].as<std::string>(),
        configYaml_["general"]["timeWait"].as<std::uint32_t>(),
        configYaml_["general"]["timeout"].as<std::uint16_t>(),
        configYaml_["general"]["repeatWait"].as<std::uint16_t>()},
      log_{
        configYaml_["log"]["logLevel"].as<std::string>(),
        configYaml_["log"]["logFile"].as<std::string>()},
      server_{
        configYaml_["server"]["threads"].as<std::uint16_t>(),
        configYaml_["server"]["listenPort"].as<std::uint16_t>(),
        configYaml_["server"]["listenIp"].as<std::string>()},
      agent_{
        configYaml_["agent"]["threads"].as<std::uint16_t>(),
        configYaml_["agent"]["listenPort"].as<std::uint16_t>(),
        configYaml_["agent"]["serverPort"].as<std::uint16_t>(),
        configYaml_["agent"]["listenIp"].as<std::string>(),
        configYaml_["agent"]["serverIp"].as<std::string>(),
        configYaml_["agent"]["token"].as<std::string>(),
        configYaml_["agent"]["httpVersion"].as<std::string>(),
        configYaml_["agent"]["userAgent"].as<std::string>()} {
    std::lock_guard<std::mutex> lock(configMutex_);
    switch (runMode_) {
        case RunMode::server:
            threadsCount_ = server_.threads;
            listenIp_ = server_.listenIp;
            listenPort_ = server_.listenPort;
            break;
        case RunMode::agent:
            threadsCount_ = agent_.threads;
            listenIp_ = agent_.listenIp;
            listenPort_ = agent_.listenPort;
            break;
    }
}

Config::Config(const Config::pointer &config)
    : runMode_(config->getRunMode()),
      configYaml_(YAML::LoadFile(config->getFilePath())),
      general_(config->getGeneralConfigs()),
      log_(config->getLogConfigs()),
      server_(config->getServerConfigs()),
      agent_(config->getAgentConfigs())
{
}


Config::~Config() = default;

std::string Config::getAllConfigsInStr() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    std::stringstream ss;
    ss << "\nConfig :\n"
       << " General :\n"
       << "   fakeUrl: " << general_.fakeUrl << "\n"
       << "   method: " << general_.method << "\n"
       << "   timeWait: " << general_.timeWait << "\n"
       << "   timeout: " << general_.timeout << "\n"
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

const Config::General& Config::getGeneralConfigs() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return general_;
}

const Config::Log& Config::getLogConfigs() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return log_;
}

const Config::Server& Config::getServerConfigs() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return server_;
}

const Config::Agent& Config::getAgentConfigs() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return agent_;
}

void Config::setThreadsCount(std::uint16_t threads) {
    std::lock_guard<std::mutex> lock(configMutex_);
    threadsCount_ = threads;
}

std::uint16_t Config::getThreadsCount() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return threadsCount_;
}

void Config::setListenIp(const std::string &ip) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenIp_ = ip;
}

const std::string &Config::getListenIp() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenIp_;
}

void Config::setListenPort(std::uint16_t port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenPort_ = port;
}

std::uint16_t Config::getListenPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenPort_;
}

RunMode Config::getRunMode() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return runMode_;
}

const std::string& Config::getFilePath() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return filePath_;
}

std::string Config::getRunModeString() const {
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
