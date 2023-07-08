#include "config.h"

using namespace std;

Config::Config(){
	config.ip 			= 	CONFIG_IP;
	config.port 		= 	CONFIG_PORT;
	config.sslKey 	= 	CONFIG_SSL_KEY;
	config.sslCert = 	CONFIG_SSL_CERT;
	config.logLevel= 	CONFIG_LOG_LEVEL;
	config.threads = 	CONFIG_THREADS;
};

Config::Config(string file){
	ifstream configFile(file, ifstream::binary);
	Json::Reader readerJson;
	Json::Value configJson;
	readerJson.parse(configFile, configJson);
	config.ip 			= configJson["ip"].toStyledString();
	config.port 		= configJson["port"].asInt();
	config.sslKey 	= configJson["sslKey"].toStyledString();
	config.sslCert = configJson["sslCert"].toStyledString();
	config.logLevel= configJson["logLevel"].asInt();
	config.threads = configJson["threads"].asInt();
	int usersSize = sizeof(configJson["users"])/sizeof(configJson["users"][0]);
	for (int item = 0; item <= usersSize; item += 1){
		config.users[item].token = configJson["users"][item]["token"].toStyledString();
		config.users[item].srcIp = configJson["users"][item]["srcIp"].toStyledString();
		config.users[item].endpoint = configJson["users"][item]["endpoint"].toStyledString();
	};
};