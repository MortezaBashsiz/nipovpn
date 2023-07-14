#include "general.hpp"

bool fileExists(string filePath){
	ifstream file;
	file.open(filePath);
	if (file) {
		file.close();
		return true;
	} else {
		return false;
	}
};

bool isInteger(string number){
	for (int i = 0; i < number.length(); i++)
	if (isdigit(number[i]) == false)
		return false;
  return true;
};

vector<string> splitString(const string &str, char delim){
	int counter = 0;
	vector<string> list; 
	auto pos = str.find(delim);
	while (pos != string::npos){
		list.push_back(str.substr(counter, pos - counter));
		counter = ++pos;
		pos = str.find(delim, pos);
	}
	list.push_back(str.substr(counter, str.length()));
	return list;
}

bool isIPAddress(string ipaddress){    
	vector<string> list = splitString(ipaddress, '.');
	if (list.size() != 4) {
		return false;
	};
	for (string str: list){
		if (!isInteger(str) || stoi(str) > 255 || stoi(str) < 0) {
			return false;
		};
	};
	return true;
};

mainArgs validateMainArgs(int argc, char* argv[]){
	mainArgs mainArgs{"null","null"};
	if (argc != 3){
		cerr << "[ERROR]: not enough arguments specified" << endl;
		cerr << "[ERROR]: run like this: " << endl;
		cerr << "[ERROR]: nipovpn [server/client] CPNFIGPATH  " << endl;
		exit(1);
	} else {
		if (std::string(argv[1]) == "server" || std::string(argv[1]) == "client"){
			mainArgs.runMode = argv[1];
		} else {
			cerr << "[ERROR]: specified argument is not correct and must be server or client: " << mainArgs.runMode << endl;
			exit(1);
		};
		mainArgs.configPath = argv[2];
		if (! fileExists(mainArgs.configPath)){
			cerr << "[ERROR]: specified config file does not exists: " << mainArgs.configPath << endl;
			exit(1);
		};
	};
	return mainArgs;
};