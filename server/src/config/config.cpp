#include "config.h"

using namespace std;

Config::Config(){
	config.ip 			= 	CONFIG_IP;
	config.port 		= 	CONFIG_PORT;
	config.sslKey 	= 	CONFIG_SSL_KEY;
	config.sslCert 	= 	CONFIG_SSL_CERT;
	config.logLevel	= 	CONFIG_LOG_LEVEL;
	config.logFile 	= 	CONFIG_LOG_FILE;
	config.threads 	= 	CONFIG_THREADS;
};

Config::Config(string file){
	ifstream configFile(file, ifstream::binary);
	Json::Reader readerJson;
	Json::Value configJson;
	readerJson.parse(configFile, configJson);
	config.ip 			= configJson["ip"].asString();
	config.port 		= configJson["port"].asInt();
	config.sslKey 	= configJson["sslKey"].asString();
	config.sslCert 	= configJson["sslCert"].asString();
	config.logLevel	= configJson["logLevel"].asInt();
	config.logFile	= configJson["logFile"].asString();
	config.threads 	= configJson["threads"].asInt();
	int usersSize 	= sizeof(configJson["users"])/sizeof(configJson["users"][0]);
	for (int item 	= 0; item <= usersSize; item += 1){
		config.users[item].token = configJson["users"][item]["token"].asString();
		config.users[item].srcIp = configJson["users"][item]["srcIp"].asString();
		config.users[item].endpoint = configJson["users"][item]["endpoint"].asString();
	};
	returnMsgCode result = validate();
	if (result.code != 0){
		cout << result.message;
		exit(result.code);
	};
};

Config::~Config(){
};

returnMsgCode Config::validate(){
	returnMsgCode result;
	if (! fileExists(config.sslKey)){
		result.message	= "[ERROR]: specified config sslKey file does not exists or is not readable : " + config.sslKey + "\n";
		result.code = 1;
	};
	if (! fileExists(config.sslCert)){
		result.message	= "[ERROR]: specified config sslCert file does not exists or is not readable : " + config.sslCert + "\n";
		result.code = 1;
	};
	if (! fileExists(config.logFile)){
		result.message	= "[ERROR]: specified config logFile file does not exists or is not readable : " + config.logFile + "\n";
		result.code = 1;
	};
	if(config.port > 65535 || config.port < 1){
		result.message	= "[ERROR]: specified config port is not correct, it must be an integer from 1 to 65535 : " + to_string(config.port) + "\n";
		result.code = 1;
	};
	if(!isIPAddress(config.ip)){
		result.message	= "[ERROR]: specified config IPv4 is not correct, it must be like an IP address : " + config.ip + "\n";
		result.code = 1;
	};
	return result;
};