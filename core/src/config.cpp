#include "config.hpp"

/**
 * @brief Constructs an Config object to manage program configuration.
 *
 * This constructor initializes the Config with the necessary resources, 
 * including read and write buffers, configuration, logging, client connection, 
 * and a unique identifier for the session.
 *
 * @param mode Reference to the RunMode which program is running.
 * @param filePath Reference to config file path.
 * 
 * @details
 * - Initializes the config directives.
 * - Sets the `runMode_` which defines the mode that program is running Server or Agent mode.
 * - Initializes the `configYaml_` which will load config gile in to variables.
 */
Config::Config(const RunMode &mode, const std::string &filePath)
    : runMode_(mode),
      filePath_(filePath),
      configYaml_(YAML::LoadFile(filePath)),
      threads_(0),
      listenIp_("127.0.0.1"),
      listenPort_(0),
      general_({configYaml_["general"]["token"].as<std::string>(),
                configYaml_["general"]["fakeUrl"].as<std::string>(),
                configYaml_["general"]["method"].as<std::string>(),
                configYaml_["general"]["timeWait"].as<unsigned int>(),
                configYaml_["general"]["timeout"].as<unsigned short>(),
                configYaml_["general"]["repeatWait"].as<unsigned short>(),
                configYaml_["general"]["chunkHeader"].as<std::string>()}),
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

/**
 * @brief Constructs an Config object from a config refrence.
 *
 * This constructor initializes the Config with the necessary resources, 
 * including read and write buffers, configuration, logging, client connection, 
 * and a unique identifier for the session.
 *
 * @param config Reference to the Config pointer.
 */
Config::Config(const Config::pointer &config)
    : runMode_(config->runMode()),
      configYaml_(YAML::LoadFile(config->filePath())),
      general_(config->general()),
      log_(config->log()),
      server_(config->server()),
      agent_(config->agent()) {}


Config::~Config() = default;

/**
 * @brief returns Config in string format to print on output.
 */
std::string Config::toString() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    std::stringstream ss;
    ss << "\nConfig :\n"
       << " General :\n"
       << "   token: " << general_.token << "\n"
       << "   fakeUrl: " << general_.fakeUrl << "\n"
       << "   method: " << general_.method << "\n"
       << "   timeWait: " << general_.timeWait << "\n"
       << "   timeout: " << general_.timeout << "\n"
       << "   repeatWait: " << general_.repeatWait << "\n"
       << "   chunkHeader: " << general_.chunkHeader << "\n"
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
       << "   httpVersion: " << agent_.httpVersion << "\n"
       << "   userAgent: " << agent_.userAgent << "\n";
    return ss.str();
}

/**
 * @brief returns General struct from Config.
 */
const Config::General &Config::general() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return general_;
}

/**
 * @brief returns Log struct from Config.
 */
const Config::Log &Config::log() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return log_;
}

/**
 * @brief returns Server struct from Config.
 */
const Config::Server &Config::server() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return server_;
}

/**
 * @brief returns Agent struct from Config.
 */
const Config::Agent &Config::agent() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return agent_;
}

/**
 * @brief returns Threads count from Config.
 */
const unsigned short &Config::threads() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return threads_;
}

/**
 * @brief sets threads count in Config to specific amount.
 * 
 * @param threads which is threads count.
 */
void Config::threads(unsigned short threads) {
    std::lock_guard<std::mutex> lock(configMutex_);
    threads_ = threads;
}

/**
 * @brief returns listenIp_ from Config.
 */
const std::string &Config::listenIp() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenIp_;
}

/**
 * @brief sets listenIp_ in config
 * 
 * @param refrence to string ip.
 */
void Config::listenIp(const std::string &ip) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenIp_ = ip;
}

/**
 * @brief returns listenPort_ from Config.
 */
const unsigned short &Config::listenPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenPort_;
}

/**
 * @brief sets listenPort_ in config
 * 
 * @param port number.
 */
void Config::listenPort(unsigned short port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenPort_ = port;
}

/**
 * @brief returns RunMode struct from Config.
 */
const RunMode &Config::runMode() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return runMode_;
}

/**
 * @brief returns filePath_ in string format.
 */
const std::string &Config::filePath() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return filePath_;
}

/**
 * @brief returns runMode in string format.
 */
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
