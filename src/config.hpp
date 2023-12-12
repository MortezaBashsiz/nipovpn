#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <yaml-cpp/yaml.h>

#include "general.hpp"

/*
* Class Config will contain all the configuration directives
* This class is declared and initialized in main.cpp
*/
class Config
{

private:

	struct Log
	{
		std::string level;
		std::string file;
	};
	
	struct Server
	{
		std::string listenIp;
		unsigned short listenPort;
		std::string webDir;
		std::string sslKey;
		std::string sslCert;
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
		std::string httpVersion;
		std::string userAgent;
	};

public:
	
	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	Config(const RunMode mode, const std::string filePath):
		mode_(mode),
		filePath_(filePath),
		configYaml_(YAML::LoadFile(filePath)),
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
	{ }

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	Config(const Config& config):
		log_(config.log_),
		server_(config.server_),
		agent_(config.agent_)
	{}

	/*
	* Default distructor
	*/
	~Config(){}
	
	/*
	* Assignment operator=
	*/
	Config& operator=(const Config& config)
	{
		log_ = config.log_;
		server_ = config.server_;
		agent_ = config.agent_;
		return *this;
	}

	/*
	* RunMode is declared in general.hpp
	*/
	RunMode mode_;

	/*
	* Configuration file path string
	*/
	std::string filePath_;

	/*
	* It contains the main yaml node of the config file
	* For more information see Config default constructor and also in general.cpp function validateConfig
	*/
	YAML::Node configYaml_;

	/*
	* Objects for separation of configuration sections (log, agent, server)
	* For more information see the private items
	*/
	Log log_;
	Server server_;
	Agent agent_;

	const std::string toString(){
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
	
};

#endif /* CONFIG_HPP */