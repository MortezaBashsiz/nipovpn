#include "general.hpp"

bool fileExists(std::string filePath){
	std::ifstream file;
	file.open(filePath);
	if (file) {
		file.close();
		return true;
	} else {
		return false;
	}
};

bool isInteger(std::string number){
	for (int i = 0; i < number.length(); i++)
	if (isdigit(number[i]) == false)
		return false;
	return true;
};

std::vector<std::string> splitString(const std::string &str, char delim){
	int counter = 0;
	std::vector<std::string> list; 
	auto pos = str.find(delim);
	while (pos != std::string::npos){
		list.push_back(str.substr(counter, pos - counter));
		counter = ++pos;
		pos = str.find(delim, pos);
	}
	list.push_back(str.substr(counter, str.length()));
	return list;
}

bool isIPAddress(std::string ipaddress){    
	std::vector<std::string> list = splitString(ipaddress, '.');
	if (list.size() != 4) {
		return false;
	};
	for (std::string str: list){
		if (!isInteger(str) || stoi(str) > 255 || stoi(str) < 0) {
			return false;
		};
	};
	return true;
};

mainArgs validateMainArgs(int argc, char* argv[]){
	mainArgs mainArgs{"null","null"};
	if (argc != 3){
		std::cerr << "[ERROR]: not enough arguments specified" << std::endl;
		std::cerr << "[ERROR]: run like this: " << std::endl;
		std::cerr << "[ERROR]: nipovpn [server/client] CPNFIGPATH  " << std::endl;
		exit(1);
	} else {
		if (std::string(argv[1]) == "server" || std::string(argv[1]) == "agent"){
			mainArgs.runMode = argv[1];
		} else {
			std::cerr << "[ERROR]: specified argument is not correct and must be server or agent: " << mainArgs.runMode << std::endl;
			exit(1);
		};
		mainArgs.configPath = argv[2];
		if (! fileExists(mainArgs.configPath)){
			std::cerr << "[ERROR]: specified config file does not exists: " << mainArgs.configPath << std::endl;
			exit(1);
		};
	};
	return mainArgs;
};