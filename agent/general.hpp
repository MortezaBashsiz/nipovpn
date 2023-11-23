#ifndef GENERAL_HPP
#define GENERAL_HPP

#include <set>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

struct returnMsgCode{
	int code;
	std::string message;
};

struct mainArgs
{
	std::string runMode;
	std::string configPath;
};

bool fileExists(std::string filePath);
bool isInteger(std::string number);
std::vector<std::string> splitString(const std::string &str, char delim);
bool isIPAddress(std::string ipaddress);
mainArgs validateMainArgs(short argc, char* argv[]);
unsigned int hexToInt(std::string hexString);
std::string hexToASCII(std::string hex);
unsigned char charToHex(char c);

#endif // GENERAL_HPP