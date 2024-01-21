#pragma once
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <yaml-cpp/yaml.h>
#include <memory>

#include "general.hpp"

/*
* This enum defines the modes which program are able to start
* Mode server and agent
*/
enum RunMode
{
	server,
	agent
};

/*
* Class Config will contain all the configuration directives
* This class is declared and initialized in main.cpp
*/
class Config 
	: private Uncopyable
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

	/*
	* RunMode is declared in general.hpp
	*/
	const RunMode& runMode_;

	/*
	* Configuration file path string
	*/
	std::string filePath_;

	/*
	* It contains the main yaml node of the config file
	* For more information see Config default constructor and also in general.cpp function validateConfig
	*/
	const YAML::Node configYaml_;

	std::string listenIp_;
	unsigned short listenPort_;

	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	explicit Config(const RunMode& mode,
		const std::string& filePath);

public:
	typedef std::shared_ptr<Config> pointer;

	static pointer create(const RunMode& mode, const std::string& filePath)
	{
		return pointer(new Config(mode, filePath));
	}

	/*
	* Objects for separation of configuration sections (log, agent, server)
	* For more information see the private items
	*/
	const Log log_;
	const Server server_;
	const Agent agent_;
	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Config(const Config::pointer& config);

	/*
	* Default distructor
	*/
	~Config();

	/*
	* Functions to handle private members
	*/
	inline const Config::Log& log() const
	{
		return log_;
	}
	inline const Config::Server& server() const
	{
		return server_;
	}
	inline const Config::Agent& agent() const
	{
		return agent_;
	}
	inline const std::string& listenIp() const
	{
		return listenIp_;
	}

	inline void listenIp(std::string ip)
	{
		listenIp_ = ip;
	}
	inline const unsigned short& listenPort() const
	{
		return listenPort_;
	}
	inline void listenPort(unsigned short port)
	{
		listenPort_ = port;
	}
	inline const RunMode& runMode() const
	{
		return runMode_;
	}
	inline const std::string& filePath() const
	{
		return filePath_;
	}
	inline const std::string modeToString() const
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

	const std::string toString() const;

};

#endif /* CONFIG_HPP */