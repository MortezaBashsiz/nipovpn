#include <vector>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

struct returnMsgCode {
	int code;
	string message;
};

bool fileExists(string filePath);
bool isInteger(string number);
vector<string> split(const string &str, char delim);
bool isIPAddress(string ipaddress);
void validateMainArgs(int argc, char* argv[]);

