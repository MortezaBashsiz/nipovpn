#include "config.hpp"

using namespace std;

Config::Config(){
	config.ip 			= 	CONFIG_IP;
	config.port 		= 	CONFIG_PORT;
	config.webDir		=		CONFIG_WEB_DIR;
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
	try{
		readerJson.parse(configFile, configJson);
		config.ip 			= configJson["ip"].asString();
		config.port 		= configJson["port"].asInt();
		config.webDir 	= configJson["webDir"].asString();
		config.sslKey 	= configJson["sslKey"].asString();
		config.sslCert 	= configJson["sslCert"].asString();
		config.logLevel	= configJson["logLevel"].asInt();
		config.logFile	= configJson["logFile"].asString();
		config.threads 	= configJson["threads"].asInt();
		for (int item 	= 0; item < CONFIG_MAX_USER_COUNT; item += 1){
			if (configJson["users"][item]["token"].asString() != ""){
				config.users[item].encryption = configJson["users"][item]["encryption"].asString();
				config.users[item].token = configJson["users"][item]["token"].asString();
				config.users[item].srcIp = configJson["users"][item]["srcIp"].asString();
				config.users[item].endpoint = configJson["users"][item]["endpoint"].asString();
			};
		};
	}	catch (const exception& error) {
		cerr << "[ERROR]: specified config file is not in json format : " << error.what() << endl;
		exit(1);
	};
	returnMsgCode result = validate();
	if (result.code != 0){
		cerr << result.message << endl;
		exit(result.code);
	};
};

Config::~Config(){
};

returnMsgCode Config::validate(){
	returnMsgCode result{0,"OK"};
	if((! isInteger(to_string(config.port))) || (isInteger(to_string(config.port)) && (config.port > 65535 || config.port < 1))){
		result.message	= "[ERROR]: specified config port is not correct, it must be an integer from 1 to 65535 : " + to_string(config.port);
		result.code = 1;
	};
	if (! isInteger(to_string(config.threads))){
		result.message	= "[ERROR]: specified config threads must be an integer : " + to_string(config.threads);
		result.code = 1;
	};
	if(! isIPAddress(config.ip)){
		result.message	= "[ERROR]: specified config IPv4 is not correct, it must be like an IP address : " + config.ip;
		result.code = 1;
	};
	if (! fileExists(config.webDir)){
		result.message	= "[ERROR]: specified config webDir file does not exists or is not readable : " + config.webDir;
		result.code = 1;
	};
	if (! fileExists(config.sslKey)){
		result.message	= "[ERROR]: specified config sslKey file does not exists or is not readable : " + config.sslKey;
		result.code = 1;
	};
	if (! fileExists(config.sslCert)){
		result.message	= "[ERROR]: specified config sslCert file does not exists or is not readable : " + config.sslCert;
		result.code = 1;
	};
	if (! fileExists(config.logFile)){
		result.message	= "[ERROR]: specified config logFile file does not exists or is not readable : " + config.logFile;
		result.code = 1;
	};
	for (int item = 0; item < CONFIG_MAX_USER_COUNT; item += 1){
		if (config.users[item].token != ""){
			if(!isIPAddress(config.users[item].srcIp)){
				result.message	= "[ERROR]: specified config IPv4 for users is not correct or no user is defined, it must be like an IP address : " + config.users[item].srcIp;
				result.code = 1;
				break;
			};
		};
	};
	return result;
};