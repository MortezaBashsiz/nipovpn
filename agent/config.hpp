#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <json/value.h>
#include <json/json.h>

#include "general.hpp"

#define SERVER_CONFIG_MAX_USER_COUNT 32

class Config
{
private:
	struct userStruct{
		std::string encryption;
		std::string token;
		std::string srcIp;
		std::string endpoint;
	};
	struct configStruct{
		std::string ip;
		unsigned short port;
		std::string webDir;
		std::string sslKey;
		std::string sslCert;
		unsigned short logLevel;
		std::string logFile;
		unsigned short threads;
		unsigned short usersCount;
		userStruct users[SERVER_CONFIG_MAX_USER_COUNT]{
			"AES_256",
			"4b445619-a2f7-48fd-9060-04252e10adee",
			"0.0.0.0",
			"api01"
		};
	};
public:
	configStruct config;
	Config();
	Config(std::string file);
	returnMsgCode validate();
	~Config();
};

#endif // CONFIG_HPP
