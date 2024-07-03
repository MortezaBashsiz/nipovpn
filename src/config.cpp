#include "config.hpp"

Config::Config(const RunMode& mode,
  const std::string& filePath)
  :
    runMode_(mode),
    filePath_(filePath),
    configYaml_(YAML::LoadFile(filePath)),
    threads_(0),
    listenIp_("127.0.0.1"),
    listenPort_(0),
    general_
    {
      configYaml_["general"]["fakeUrl"].as<std::string>(),
      configYaml_["general"]["method"].as<std::string>()
    },
    log_
    {
      configYaml_["log"]["logLevel"].as<std::string>(),
      configYaml_["log"]["logFile"].as<std::string>()
    },
    server_
    {
      configYaml_["server"]["threads"].as<unsigned short>(),
      configYaml_["server"]["listenIp"].as<std::string>(),
      configYaml_["server"]["listenPort"].as<unsigned short>(),
      configYaml_["server"]["webDir"].as<std::string>(),
      configYaml_["server"]["sslKey"].as<std::string>(),
      configYaml_["server"]["sslCert"].as<std::string>()
    },
    agent_
    {
      configYaml_["agent"]["threads"].as<unsigned short>(),
      configYaml_["agent"]["listenIp"].as<std::string>(),
      configYaml_["agent"]["listenPort"].as<unsigned short>(),
      configYaml_["agent"]["serverIp"].as<std::string>(),
      configYaml_["agent"]["serverPort"].as<unsigned short>(),
      configYaml_["agent"]["encryption"].as<std::string>(),
      configYaml_["agent"]["token"].as<std::string>(),
      configYaml_["agent"]["endPoint"].as<std::string>(),
      configYaml_["agent"]["httpVersion"].as<std::string>(),
      configYaml_["agent"]["userAgent"].as<std::string>()
    }
{
  switch (runMode_){
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

Config::Config(const Config::pointer& config)
  :
    runMode_(config->runMode()),
    configYaml_(YAML::LoadFile(config->filePath())),
    log_(config->log()),
    server_(config->server()),
    agent_(config->agent())
{ }

Config::~Config()
{ }

const std::string Config::toString() const
{
  return std::string("\nConfig :\n")
  + " General :\n"
  + "   fakeUrl: " + general_.fakeUrl + "\n"
  + "   method: " + general_.method + "\n"
  + " Log :\n"
  + "   logLevel: " + log_.level + "\n"
  + "   logFile: " + log_.file + "\n"
  + " server :\n"
  + "   threads: " + std::to_string(server_.threads) + "\n"
  + "   listenIp: " + server_.listenIp + "\n"
  + "   listenPort: " + std::to_string(server_.listenPort) + "\n"
  + "   webDir: " + server_.webDir + "\n"
  + "   sslKey: " + server_.sslKey + "\n"
  + "   sslCert: " + server_.sslCert + "\n"
  + " agent :\n"
  + "   threads: " + std::to_string(agent_.threads) + "\n"
  + "   listenIp: " + agent_.listenIp + "\n"
  + "   listenPort: " + std::to_string(agent_.listenPort) + "\n"
  + "   serverIp: " + agent_.serverIp + "\n"
  + "   serverPort: " + std::to_string(agent_.serverPort) + "\n"
  + "   encryption: " + agent_.encryption + "\n"
  + "   token: " + agent_.token + "\n"
  + "   endPoint: " + agent_.endPoint + "\n"
  + "   httpVersion: " + agent_.httpVersion + "\n"
  + "   userAgent: " + agent_.userAgent + "\n";
}