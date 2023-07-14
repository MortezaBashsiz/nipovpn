#ifndef GENERAL_HPP
#define GENERAL_HPP

#include <vector>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

struct returnMsgCode{
	int code;
	string message;
};

struct mainArgs{
	string runMode;
	string configPath;
};


bool fileExists(string filePath);
bool isInteger(string number);
vector<string> split(const string &str, char delim);
bool isIPAddress(string ipaddress);
mainArgs validateMainArgs(int argc, char* argv[]);

#endif // GENERAL_HPP