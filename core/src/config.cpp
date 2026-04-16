#include "config.hpp"

/**
 * @brief Constructs a `Config` instance by loading and parsing a YAML configuration file.
 *
 * @details
 * - Loads the YAML configuration from the provided file path.
 * - Initializes all configuration sections (`general_`, `log_`, `server_`, `agent_`)
 *   directly from the parsed YAML structure.
 * - Sets default runtime values for threads, listen IP, and port.
 * - Based on the selected `RunMode`, initializes runtime parameters:
 *   - `threads_`
 *   - `listenIp_`
 *   - `listenPort_`
 * - Ensures thread-safe initialization using a mutex lock.
 *
 * @param mode Runtime mode (server or agent).
 * @param filePath Path to the YAML configuration file.
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
                configYaml_["general"]["timeout"].as<unsigned short>(),
                configYaml_["general"]["chunkHeader"].as<std::string>()}),
      log_({configYaml_["log"]["logLevel"].as<std::string>(),
            configYaml_["log"]["logFile"].as<std::string>()}),
      server_({configYaml_["server"]["threads"].as<unsigned short>(),
               configYaml_["server"]["listenIp"].as<std::string>(),
               configYaml_["server"]["listenPort"].as<unsigned short>(),
               configYaml_["server"]["tlsEnable"].as<bool>(false),
               configYaml_["server"]["tlsVerifyPeer"].as<bool>(false),
               configYaml_["server"]["tlsCertFile"].as<std::string>(""),
               configYaml_["server"]["tlsKeyFile"].as<std::string>(""),
               configYaml_["server"]["tlsCaFile"].as<std::string>("")}),
      agent_({configYaml_["agent"]["threads"].as<unsigned short>(),
              configYaml_["agent"]["listenIp"].as<std::string>(),
              configYaml_["agent"]["listenPort"].as<unsigned short>(),
              configYaml_["agent"]["serverIp"].as<std::string>(),
              configYaml_["agent"]["serverPort"].as<unsigned short>(),
              configYaml_["agent"]["httpVersion"].as<std::string>(),
              configYaml_["agent"]["userAgent"].as<std::string>(),
              configYaml_["agent"]["tlsEnable"].as<bool>(false),
              configYaml_["agent"]["tlsVerifyPeer"].as<bool>(false),
              configYaml_["agent"]["tlsCaFile"].as<std::string>("")}) {

    std::lock_guard<std::mutex> lock(configMutex_);

    /**
     * @brief Initializes runtime parameters based on the selected run mode.
     */
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
 * @brief Copy constructor that creates a new `Config` instance from an existing one.
 *
 * @details
 * - Copies all configuration sections and runtime parameters.
 * - Reloads the YAML configuration from the original file path.
 * - Preserves the runtime mode and all derived values.
 *
 * @param config Shared pointer to the existing configuration.
 */
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

/**
 * @brief Destructor for `Config`.
 *
 * @details
 * - Defaulted destructor; no explicit resource cleanup required.
 */
Config::~Config() = default;

/**
 * @brief Converts the configuration into a human-readable string.
 *
 * @details
 * - Serializes all configuration sections into a formatted string.
 * - Includes General, Log, Server, and Agent sections.
 * - Useful for debugging and logging configuration state.
 *
 * @return A formatted string representation of the configuration.
 */
std::string Config::toString() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    std::stringstream ss;

    ss << "\nConfig :\n"
       << " General :\n"
       << "   token: " << general_.token << "\n"
       << "   fakeUrl: " << general_.fakeUrl << "\n"
       << "   method: " << general_.method << "\n"
       << "   timeout: " << general_.timeout << "\n"
       << "   chunkHeader: " << general_.chunkHeader << "\n"
       << " Log :\n"
       << "   logLevel: " << log_.level << "\n"
       << "   logFile: " << log_.file << "\n"
       << " server :\n"
       << "   threads: " << server_.threads << "\n"
       << "   listenIp: " << server_.listenIp << "\n"
       << "   listenPort: " << server_.listenPort << "\n"
       << "   tlsEnable: " << server_.tlsEnable << "\n"
       << "   tlsVerifyPeer: " << server_.tlsVerifyPeer << "\n"
       << "   tlsCertFile: " << server_.tlsCertFile << "\n"
       << "   tlsKeyFile: " << server_.tlsKeyFile << "\n"
       << "   tlsCaFile: " << server_.tlsCaFile << "\n"
       << " agent :\n"
       << "   threads: " << agent_.threads << "\n"
       << "   listenIp: " << agent_.listenIp << "\n"
       << "   listenPort: " << agent_.listenPort << "\n"
       << "   serverIp: " << agent_.serverIp << "\n"
       << "   serverPort: " << agent_.serverPort << "\n"
       << "   httpVersion: " << agent_.httpVersion << "\n"
       << "   userAgent: " << agent_.userAgent << "\n"
       << "   tlsEnable: " << agent_.tlsEnable << "\n"
       << "   tlsVerifyPeer: " << agent_.tlsVerifyPeer << "\n"
       << "   tlsCaFile: " << agent_.tlsCaFile << "\n";

    return ss.str();
}

/**
 * @brief Returns the General configuration section.
 */
const Config::General &Config::general() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return general_;
}

/**
 * @brief Returns the Log configuration section.
 */
const Config::Log &Config::log() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return log_;
}

/**
 * @brief Returns the Server configuration section.
 */
const Config::Server &Config::server() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return server_;
}

/**
 * @brief Returns the Agent configuration section.
 */
const Config::Agent &Config::agent() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return agent_;
}

/**
 * @brief Gets the current thread count.
 */
const unsigned short &Config::threads() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return threads_;
}

/**
 * @brief Sets the thread count at runtime.
 */
void Config::threads(unsigned short threads) {
    std::lock_guard<std::mutex> lock(configMutex_);
    threads_ = threads;
}

/**
 * @brief Gets the listening IP address.
 */
const std::string &Config::listenIp() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenIp_;
}

/**
 * @brief Sets the listening IP address at runtime.
 */
void Config::listenIp(const std::string &ip) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenIp_ = ip;
}

/**
 * @brief Gets the listening port.
 */
const unsigned short &Config::listenPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return listenPort_;
}

/**
 * @brief Sets the listening port at runtime.
 */
void Config::listenPort(unsigned short port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    listenPort_ = port;
}

/**
 * @brief Returns the current runtime mode.
 */
const RunMode &Config::runMode() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return runMode_;
}

/**
 * @brief Returns the configuration file path.
 */
const std::string &Config::filePath() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return filePath_;
}

/**
 * @brief Converts the runtime mode to a string.
 *
 * @return "server" or "agent" depending on the current mode.
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