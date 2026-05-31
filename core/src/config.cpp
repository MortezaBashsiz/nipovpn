#include "config.hpp"


Config::Config(const RunMode &mode, const std::string &filePath)
    : runMode_(mode),
      filePath_(filePath),
      configYaml_(YAML::LoadFile(filePath)),
      threads_(0),
      listenIp_("127.0.0.1"),
      listenPort_(0),
      general_({configYaml_["general"]["token"].as<std::string>(),
                configYaml_["general"]["fakeUrls"].as<std::vector<std::string>>(),
                configYaml_["general"]["methods"].as<std::vector<std::string>>(),
                configYaml_["general"]["endPoints"].as<std::vector<std::string>>(),
                configYaml_["general"]["timeout"].as<unsigned short>(),
                configYaml_["general"]["tunnelEnable"].as<bool>(false),
                configYaml_["general"]["connectionReuse"].as<bool>(false),
                configYaml_["general"]["tlsEnable"].as<bool>(false),
                configYaml_["general"]["tlsVerifyPeer"].as<bool>(false),
                configYaml_["general"]["tlsCertFile"].as<std::string>(""),
                configYaml_["general"]["tlsKeyFile"].as<std::string>(""),
                configYaml_["general"]["tlsCaFile"].as<std::string>("")}),
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

Config::Config(const Config::pointer &config)
    : runMode_(config->runMode()),
      filePath_(config->filePath()),
      configYaml_(YAML::LoadFile(config->filePath())),
      threads_(config->threads()),
      listenIp_(config->listenIp()),
      listenPort_(config->listenPort()),
      general_(config->general()),
      log_(config->log()),
      server_(config->server()),
      agent_(config->agent()) {}

Config::~Config() = default;

std::string Config::toString() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    std::stringstream ss;

    ss << "\nConfig :\n"
       << " General :\n"
       << "   token: " << general_.token << "\n"
       << "   fakeUrls: ";

    for (std::size_t i = 0; i < general_.fakeUrls.size(); ++i) {
        ss << general_.fakeUrls[i];

        if (i != general_.fakeUrls.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   methods: ";

    for (std::size_t i = 0; i < general_.methods.size(); ++i) {
        ss << general_.methods[i];

        if (i != general_.methods.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   endPoints: ";

    for (std::size_t i = 0; i < general_.endPoints.size(); ++i) {
        ss << general_.endPoints[i];

        if (i != general_.endPoints.size() - 1) {
            ss << ", ";
        }
    }

    ss << "\n"
       << "   timeout: " << general_.timeout << "\n"
       << "   tunnelEnable: " << std::boolalpha << general_.tunnelEnable << "\n"
       << "   connectionReuse: " << std::boolalpha << general_.connectionReuse << "\n"
       << "   tlsEnable: " << std::boolalpha << general_.tlsEnable << "\n"
       << "   tlsVerifyPeer: " << std::boolalpha << general_.tlsVerifyPeer << "\n"
       << "   tlsCertFile: " << general_.tlsCertFile << "\n"
       << "   tlsKeyFile: " << general_.tlsKeyFile << "\n"
       << "   tlsCaFile: " << general_.tlsCaFile << "\n"

       << " Log :\n"
       << "   logLevel: " << log_.level << "\n"
       << "   logFile: " << log_.file << "\n"

       << " Server :\n"
       << "   threads: " << server_.threads << "\n"
       << "   listenIp: " << server_.listenIp << "\n"
       << "   listenPort: " << server_.listenPort << "\n"

       << " Agent :\n"
       << "   threads: " << agent_.threads << "\n"
       << "   listenIp: " << agent_.listenIp << "\n"
       << "   listenPort: " << agent_.listenPort << "\n"
       << "   serverIp: " << agent_.serverIp << "\n"
       << "   serverPort: " << agent_.serverPort << "\n"
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

std::string Config::randomFakeUrl() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (general_.fakeUrls.empty()) {
        return "www.google.com";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.fakeUrls.size() - 1);

    return general_.fakeUrls[distribution(generator)];
}

std::string Config::randomMethod() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (general_.methods.empty()) {
        return "GET";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.methods.size() - 1);

    return general_.methods[distribution(generator)];
}

std::string Config::randomEndPoint() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (general_.endPoints.empty()) {
        return "api";
    }

    static thread_local std::mt19937 generator{std::random_device{}()};

    std::uniform_int_distribution<std::size_t> distribution(
            0,
            general_.endPoints.size() - 1);

    return general_.endPoints[distribution(generator)];
}