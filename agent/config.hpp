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
		std::string token;
		int salt;
		std::string endPoint;
		unsigned short httpVersion;
		std::string userAgent;
	};
public:
	configStruct config;
	Config();
	Config(std::string file);
	returnMsgCode validate();
	~Config();
};

#endif // CONFIG_HPP
