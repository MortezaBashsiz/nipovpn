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

mainArgs validateMainArgs(short argc, char* argv[])
{
	mainArgs mainArgs{"null","null"};
	mainArgs.configPath = argv[1];
	if (! fileExists(mainArgs.configPath))
	{
		std::cerr << "[ERROR]: specified config file does not exists: " << mainArgs.configPath << std::endl;
		exit(1);
	}
	return mainArgs;
};

unsigned int hexToInt(std::string hexString){
	std::stringstream hexStr;
	unsigned int result;
	hexStr << std::hex << hexString;
	hexStr >> result;
	return result;
}

std::string hexToASCII(std::string hex){
	std::string ascii = "";
	for (size_t i = 0; i < hex.length(); i += 2){
		std::string part = hex.substr(i, 2);
		char ch = stoul(part, nullptr, 16);
		ascii += ch;
	}
	return ascii;
}

std::string hexArrToStr(unsigned char* data){
	std::stringstream tempStr;
	tempStr << std::hex << std::setfill('0');
	for (int i = 0; i < sizeof(data); ++i)
	{
		tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
	}
	return tempStr.str();
}