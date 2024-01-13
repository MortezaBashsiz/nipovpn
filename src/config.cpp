#include "config.hpp"

Config::Config(const RunMode& mode,
	const std::string& filePath)
	:
		runMode_(mode),
		filePath_(filePath),
		configYaml_(YAML::LoadFile(filePath)),
		listenIp_("127.0.0.1"),
		listenPort_(0),
		log_
		{
			configYaml_["log"]["logLevel"].as<std::string>(),
			configYaml_["log"]["logFile"].as<std::string>()
		},
		server_
		{
			configYaml_["server"]["listenIp"].as<std::string>(),
			configYaml_["server"]["listenPort"].as<unsigned short>(),
			configYaml_["server"]["webDir"].as<std::string>(),
			configYaml_["server"]["sslKey"].as<std::string>(),
			configYaml_["server"]["sslCert"].as<std::string>()
		},
		agent_
		{
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
		listenIp_ = server_.listenIp;
		listenPort_ = server_.listenPort;
		break;
	case RunMode::agent:
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
{	}

Config::~Config()
{ }

const Config::Log& Config::log() const
{
	return log_;
}
const Config::Server& Config::server() const
{
	return server_;
}
const Config::Agent& Config::agent() const
{
	return agent_;
}
const std::string& Config::listenIp() const
{
	return listenIp_;
}
void Config::listenIp(std::string ip)
{
	listenIp_ = ip;
}
const unsigned short& Config::listenPort() const
{
	return listenPort_;
}
void Config::listenPort(unsigned short port)
{
	listenPort_ = port;
}
const RunMode& Config::runMode() const
{
	return runMode_;
}
const std::string& Config::filePath() const
{
	return filePath_;
}
const std::string Config::modeToString() const
{
	switch (runMode_){
		case RunMode::server:
			return "server";
			break;
		case RunMode::agent:
			return "agent";
			break;
		default:
			return "UNKNOWN MODE";
	}
}
const std::string Config::toString() const
{
	return std::string("\nConfig :\n")
	+ "	Log :\n"
	+ "		logLevel: " + log_.level + "\n"
	+ "		logFile: " + log_.file + "\n"
	+ "	server :\n"
	+ "		listenIp: " + server_.listenIp + "\n"
	+ "		listenPort: " + std::to_string(server_.listenPort) + "\n"
	+ "		webDir: " + server_.webDir + "\n"
	+ "		sslKey: " + server_.sslKey + "\n"
	+ "		sslCert: " + server_.sslCert + "\n"
	+ "	agent :\n"
	+ "		listenIp: " + agent_.listenIp + "\n"
	+ "		listenPort: " + std::to_string(agent_.listenPort) + "\n"
	+ "		serverIp: " + agent_.serverIp + "\n"
	+ "		serverPort: " + std::to_string(agent_.serverPort) + "\n"
	+ "		encryption: " + agent_.encryption + "\n"
	+ "		token: " + agent_.token + "\n"
	+ "		endPoint: " + agent_.endPoint + "\n"
	+ "		httpVersion: " + agent_.httpVersion + "\n"
	+ "		userAgent: " + agent_.userAgent + "\n";
}