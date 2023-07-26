#include "config.hpp"

using namespace std;

Config::Config(){
};

Config::Config(string file){
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
		config.token	= configJson["token"].asString();
		config.salt	= configJson["salt"].asInt();
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

Config::~Config(){
};

returnMsgCode Config::validate(){
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