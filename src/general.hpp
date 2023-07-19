#ifndef GENERAL_HPP
#define GENERAL_HPP

#include <vector>
#include <fstream>
#include <iostream>
#include <string>

#include "header.hpp"


struct returnMsgCode{
	int code;
	std::string message;
};

struct mainArgs{
	std::string runMode;
	std::string configPath;
};


bool fileExists(std::string filePath);
bool isInteger(std::string number);
std::vector<std::string> split(const std::string &str, char delim);
bool isIPAddress(std::string ipaddress);
mainArgs validateMainArgs(int argc, char* argv[]);

#endif // GENERAL_HPP