#include "config.hpp"

using namespace std;

serverConfig::serverConfig(){
	config.ip 				= 	SERVER_CONFIG_IP;
	config.port 			= 	SERVER_CONFIG_PORT;
	config.webDir			=		SERVER_CONFIG_WEB_DIR;
	config.sslKey			= 	SERVER_CONFIG_SSL_KEY;
	config.sslCert 		= 	SERVER_CONFIG_SSL_CERT;
	config.logLevel		= 	SERVER_CONFIG_LOG_LEVEL;
	config.logFile 		= 	SERVER_CONFIG_LOG_FILE;
	config.threads 		= 	SERVER_CONFIG_THREADS;
	config.usersCount = 	SERVER_CONFIG_USERS_COUNT;
};

serverConfig::serverConfig(string file){
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
		config.usersCount = end(configJson["users"])-begin(configJson["users"]); 
		for (int item 	= 0; item < config.usersCount; item += 1){
			if (configJson["users"][item]["token"].asString() != ""){
				config.users[item].encryption = configJson["users"][item]["encryption"].asString();
				config.users[item].token = configJson["users"][item]["token"].asString();
				config.users[item].srcIp = configJson["users"][item]["srcIp"].asString();
				config.users[item].endpoint = configJson["users"][item]["endpoint"].asString();
			};
		};
	}	catch (const exception& error) {
		std::cerr << "[ERROR]: specified config file is not in json format : " << error.what() << std::endl;
		exit(1);
	};
	returnMsgCode result = validate();
	if (result.code != 0){
		std::cerr << result.message << std::endl;
		exit(result.code);
	};
};

serverConfig::~serverConfig(){
};

returnMsgCode serverConfig::validate(){
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
	for (int item = 0; item < SERVER_CONFIG_MAX_USER_COUNT; item += 1){
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

agentConfig::agentConfig(){
	config.localIp		=	AGENT_CONFIG_LOCAL_IP;
	config.localPort	=	AGENT_CONFIG_LOCAL_PORT;
	config.serverIp		=	AGENT_CONFIG_SERVER_IP;
	config.serverPort	=	AGENT_CONFIG_SERVER_PORT;
	config.logLevel		=	AGENT_CONFIG_LOG_LEVEL;
	config.logFile		=	AGENT_CONFIG_LOG_FILE;
	config.encryption	=	AGENT_CONFIG_ENCRYPTION;
};

agentConfig::agentConfig(string file){
	ifstream configFile(file, ifstream::binary);
	Json::Reader readerJson;
	Json::Value configJson;
	try{
		readerJson.parse(configFile, configJson);
		config.localIp	= configJson["localIp"].asString();
		config.localPort	= configJson["localPort"].asInt();
		config.serverIp	= configJson["serverIp"].asString();
		config.serverPort	= configJson["serverPort"].asInt();
		config.logLevel	= configJson["logLevel"].asInt();
		config.logFile	= configJson["logFile"].asString();
		config.encryption	= configJson["encryption"].asString();
	}	catch (const exception& error) {
		std::cerr << "[ERROR]: specified config file is not in json format : " << error.what() << std::endl;
		exit(1);
	};
	returnMsgCode result = validate();
	if (result.code != 0){
		std::cerr << result.message << std::endl;
		exit(result.code);
	};
};

agentConfig::~agentConfig(){
};

returnMsgCode agentConfig::validate(){
	returnMsgCode result{0,"OK"};
	if((! isInteger(to_string(config.localPort))) || (isInteger(to_string(config.localPort)) && (config.localPort > 65535 || config.localPort < 1))){
		result.message	= "[ERROR]: specified config localPort is not correct, it must be an integer from 1 to 65535 : " + to_string(config.localPort);
		result.code = 1;
	};
	if((! isInteger(to_string(config.serverPort))) || (isInteger(to_string(config.serverPort)) && (config.serverPort > 65535 || config.serverPort < 1))){
		result.message	= "[ERROR]: specified config serverPort is not correct, it must be an integer from 1 to 65535 : " + to_string(config.serverPort);
		result.code = 1;
	};
	if(! isIPAddress(config.localIp)){
		result.message	= "[ERROR]: specified config localIp is not correct, it must be like an IP address : " + config.localIp;
		result.code = 1;
	};
	if(! isIPAddress(config.serverIp)){
		result.message	= "[ERROR]: specified config serverIp is not correct, it must be like an IP address : " + config.serverIp;
		result.code = 1;
	};
	if (! fileExists(config.logFile)){
		result.message	= "[ERROR]: specified config logFile file does not exists or is not readable : " + config.logFile;
		result.code = 1;
	};
	return result;
};