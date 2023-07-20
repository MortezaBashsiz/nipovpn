#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <iostream>
#include <json/value.h>
#include <json/json.h>

#include "general.hpp"

#define SERVER_CONFIG_MAX_USER_COUNT 32 
#define SERVER_CONFIG_IP "0.0.0.0" 
#define SERVER_CONFIG_PORT 443 
#define SERVER_CONFIG_WEB_DIR "/var/lib/nipvn" 
#define SERVER_CONFIG_SSL_KEY "/etc/nipovpn/ssl.key" 
#define SERVER_CONFIG_SSL_CERT "/etc/nipovpn/ssl.cert" 
#define SERVER_CONFIG_LOG_LEVEL 1 
#define SERVER_CONFIG_LOG_FILE "/var/log/niposerver/nipo.log" 
#define SERVER_CONFIG_THREADS 1 
#define SERVER_CONFIG_USERS_COUNT 1

#define AGENT_CONFIG_LOCAL_IP "127.0.0.1"
#define AGENT_CONFIG_LOCAL_PORT 8181
#define AGENT_CONFIG_SERVER_IP "127.0.0.1"
#define AGENT_CONFIG_SERVER_PORT 80
#define AGENT_CONFIG_LOG_LEVEL 1
#define AGENT_CONFIG_LOG_FILE "/var/log/nipoagent/nipo.log" 
#define AGENT_CONFIG_ENCRYPTION "AES_256"

class serverConfig{
	private:
		struct userStruct{
			std::string encryption;
			std::string token;
			std::string srcIp;
			std::string endpoint;
		};
		struct configStruct{
			std::string ip;
			int port;
			std::string webDir;
			std::string sslKey;
			std::string sslCert;
			int logLevel;
			std::string logFile;
			int threads;
			int usersCount;
			userStruct users[SERVER_CONFIG_MAX_USER_COUNT]{
				"AES_256",
				"4b445619-a2f7-48fd-9060-04252e10adee",
				"0.0.0.0",
				"api01"
			};
		};
	public:
		configStruct config;
		serverConfig();
		serverConfig(std::string file);
		returnMsgCode validate();
		~serverConfig();
};

class agentConfig{
	private:
		struct configStruct{
			std::string localIp;
			int localPort;
			std::string serverIp;
			int serverPort;
			int logLevel;
			std::string logFile;
			std::string encryption;
			std::string token;
			std::string endpoint;
		};
	public:
		configStruct config;
		agentConfig();
		agentConfig(std::string file);
		returnMsgCode validate();
		~agentConfig();
};

#endif // CONFIG_HPP