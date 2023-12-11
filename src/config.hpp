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
	
	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	Config(const RunMode mode, const std::string filePath):
		mode_(mode),
		filePath_(filePath),
		configYaml_(YAML::LoadFile(filePath)),
		log_(),
		agent_(),
		server_()
	{
		log_.level = configYaml_["log"]["logLevel"].as<std::string>();
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

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	Config(const Config& config_):
		log_(config_.log_),
		agent_(config_.agent_),
		server_(config_.server_)
	{}

	/*
	* Default distructor
	*/
	~Config(){}
	
	/*
	* Assignment operator=
	*/
	Config& operator=(const Config& config_)
	{
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
	Agent agent_;
	Server server_;
	
};

#endif /* CONFIG_HPP */