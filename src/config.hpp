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
		logYaml_(configYaml_["log"]),
		agentYaml_(configYaml_["agent"]),
		serverYaml_(configYaml_["server"]),
		log_(),
		agent_(),
		server_()
	{
		log_.level = logYaml_["logLevel"].as<unsigned short>();
		log_.file = logYaml_["logFile"].as<std::string>();
		agent_.listenIp = agentYaml_["listenIp"].as<std::string>();
		agent_.listenPort = agentYaml_["listenPort"].as<unsigned short>();
		agent_.serverIp = agentYaml_["serverIp"].as<std::string>();
		agent_.serverPort = agentYaml_["serverPort"].as<unsigned short>();
		agent_.encryption = agentYaml_["encryption"].as<std::string>();
		agent_.token = agentYaml_["token"].as<std::string>();
		agent_.endPoint = agentYaml_["endPoint"].as<std::string>();
		agent_.httpVersion = agentYaml_["httpVersion"].as<unsigned short>();
		agent_.userAgent = agentYaml_["userAgent"].as<std::string>();
		server_.listenIp = serverYaml_["listenIp"].as<std::string>();
		server_.listenPort = serverYaml_["listenPort"].as<unsigned short>();
		server_.webDir = serverYaml_["webDir"].as<std::string>();
		server_.sslKey = serverYaml_["sslKey"].as<std::string>();
		server_.sslCert = serverYaml_["sslCert"].as<std::string>();
	}

	Config(const Config& config_){}
	~Config(){}
	Config& operator=(const Config& config_)
	{
		return *this;
	}

	RunMode mode_;
	std::string filePath_;
	YAML::Node configYaml_, logYaml_, agentYaml_, serverYaml_;
	Log log_;
	Agent agent_;
	Server server_;

};