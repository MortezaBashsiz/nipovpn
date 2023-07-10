#include <vector>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

bool fileExists(string filePath);
bool checkInteger(string number);
vector<string> split(const string &str, char delim);
bool isIPAddress(string ipaddress);

struct returnMsgCode {
	int code;
	string message;
};