#include <yaml-cpp/yaml.h>

#include "general.hpp"

class Config
{

private:

	struct Log
	{
		unsigned short level;
		std::string file;
	};

	struct Agent
	{
		std::string listenIp;
		unsigned short listenPort;
		std::string serverIp;
		unsigned short serverPort;
		std::string encryption;
		std::string token;
		std::string endPoint;
		unsigned short httpVersion;
		std::string userAgent;
	};

	struct Server
	{
		std::string listenIp;
		unsigned short listenPort;
		std::string webDir;
		std::string sslKey;
		std::string sslCert;
	};

public:
	
	Config(const RunMode mode, const std::string filePath):
		mode_(mode),
		filePath_(filePath),
		configYaml_(YAML::LoadFile(filePath)),
		log_(),
		agent_(),
		server_()
	{
		log_.level = configYaml_["log"]["logLevel"].as<unsigned short>();
		log_.file = configYaml_["log"]["logFile"].as<std::string>();
		agent_.listenIp = configYaml_["agent"]["listenIp"].as<std::string>();
		agent_.listenPort = configYaml_["agent"]["listenPort"].as<unsigned short>();
		agent_.serverIp = configYaml_["agent"]["serverIp"].as<std::string>();
		agent_.serverPort = configYaml_["agent"]["serverPort"].as<unsigned short>();
		agent_.encryption = configYaml_["agent"]["encryption"].as<std::string>();
		agent_.token = configYaml_["agent"]["token"].as<std::string>();
		agent_.endPoint = configYaml_["agent"]["endPoint"].as<std::string>();
		agent_.httpVersion = configYaml_["agent"]["httpVersion"].as<unsigned short>();
		agent_.userAgent = configYaml_["agent"]["userAgent"].as<std::string>();
		server_.listenIp = configYaml_["server"]["listenIp"].as<std::string>();
		server_.listenPort = configYaml_["server"]["listenPort"].as<unsigned short>();
		server_.webDir = configYaml_["server"]["webDir"].as<std::string>();
		server_.sslKey = configYaml_["server"]["sslKey"].as<std::string>();
		server_.sslCert = configYaml_["server"]["sslCert"].as<std::string>();
	}

	Config(const Config& config_){}
	~Config(){}
	Config& operator=(const Config& config_)
	{
		return *this;
	}

	RunMode mode_;
	std::string filePath_;
	YAML::Node configYaml_;
	Log log_;
	Agent agent_;
	Server server_;
	
};