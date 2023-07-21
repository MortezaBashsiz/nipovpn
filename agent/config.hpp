#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <json/value.h>
#include <json/json.h>

#include "general.hpp"

class Config
{
private:
	struct configStruct{
		std::string localIp;
		unsigned short localPort;
		std::string serverIp;
		unsigned short serverPort;
		unsigned short logLevel;
		std::string logFile;
		std::string encryption;
		std::string token;
		std::string endpoint;
	};
public:
	configStruct config;
	Config();
	Config(std::string file);
	returnMsgCode validate();
	~Config();
};

#endif // CONFIG_HPP
