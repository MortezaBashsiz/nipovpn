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
  // Set specific parameters based on the operational mode.
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

/**
 * @brief Returns the general configuration settings.
 *
 * @return A constant reference to the general configuration.
 */
const Config::General &Config::general() const { return general_; }

/**
 * @brief Returns the logging configuration settings.
 *
 * @return A constant reference to the logging configuration.
 */
const Config::Log &Config::log() const { return log_; }

/**
 * @brief Returns the server configuration settings.
 *
 * @return A constant reference to the server configuration.
 */
const Config::Server &Config::server() const { return server_; }

/**
 * @brief Returns the agent configuration settings.
 *
 * @return A constant reference to the agent configuration.
 */
const Config::Agent &Config::agent() const { return agent_; }

/**
 * @brief Returns the number of threads specified in the configuration.
 *
 * @return A constant reference to the number of threads.
 */
const unsigned short &Config::threads() const { return threads_; }

/**
 * @brief Sets the number of threads in the configuration.
 *
 * @param threads The number of threads to set.
 */
void Config::threads(unsigned short threads) { threads_ = threads; }

/**
 * @brief Returns the IP address on which the server listens.
 *
 * @return A constant reference to the IP address.
 */
const std::string &Config::listenIp() const { return listenIp_; }

/**
 * @brief Sets the IP address on which the server listens.
 *
 * @param ip The IP address to set.
 */
void Config::listenIp(const std::string &ip) { listenIp_ = ip; }

/**
 * @brief Returns the port on which the server listens.
 *
 * @return A constant reference to the port number.
 */
const unsigned short &Config::listenPort() const { return listenPort_; }

/**
 * @brief Sets the port on which the server listens.
 *
 * @param port The port number to set.
 */
void Config::listenPort(unsigned short port) { listenPort_ = port; }

/**
 * @brief Returns the current operational mode of the application.
 *
 * @return A constant reference to the operational mode.
 */
const RunMode &Config::runMode() const { return runMode_; }

/**
 * @brief Returns the file path to the configuration file.
 *
 * @return A constant reference to the file path.
 */
const std::string &Config::filePath() const { return filePath_; }

/**
 * @brief Converts the run mode to a string representation.
 *
 * @return A string describing the operational mode.
 */
std::string Config::modeToString() const {
  switch (runMode_) {
    case RunMode::server:
      return "server";
    case RunMode::agent:
      return "agent";
    default:
      return "UNKNOWN MODE";
  }
}
