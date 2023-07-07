#include <string.h>
#include <fstream>
#include <iostream>
#include <json/value.h>
#include <json/json.h>

#include "../env.h"

using namespace std;

class Config{
	private:
		struct user{
			string token;
			string srcIp;
			string endpoint;
		};

	public:
		string ip;
		int port;
		string sslKey;
		string sslCert;
		int logLevel;
		int threads;
		user users[CONFIG_MAX_USER_COUNT]{
			"a30929e7-284a-448c-a7f1-e91b986cd8f5",
			"0.0.0.0",
			"api01"
		};
		Config();
		Config(string file);
};

Config::Config(){
	ip 		= 	CONFIG_IP;
	port 	= 	CONFIG_PORT;
	sslKey 	= 	CONFIG_SSL_KEY;
	sslCert = 	CONFIG_SSL_CERT;
	logLevel= 	CONFIG_LOG_LEVEL;
	threads = 	CONFIG_THREADS;
};

Config::Config(string file){
	ifstream configFile(file, ifstream::binary);
	Json::Reader readerJson;
	Json::Value configJson;
	readerJson.parse(configFile, configJson);
	ip 		= configJson["ip"].toStyledString();
	port 	= configJson["port"].asInt();
	sslKey 	= configJson["sslKey"].toStyledString();
	sslCert = configJson["sslCert"].toStyledString();
	logLevel= configJson["logLevel"].asInt();
	threads = configJson["threads"].asInt();
	int usersSize = sizeof(configJson["users"])/sizeof(configJson["users"][0]);
	for (int item = 0; item <= usersSize; item += 1){
		users[item].token = configJson["users"][item]["token"].toStyledString();
		users[item].srcIp = configJson["users"][item]["srcIp"].toStyledString();
		users[item].endpoint = configJson["users"][item]["endpoint"].toStyledString();
	}
};




