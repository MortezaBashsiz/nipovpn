#ifndef LOG_HPP
#define LOG_HPP

#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

using namespace std;

class Log{
	private:
		struct logStruct{
			string file;
			int level;
		};
	public:
		const struct typeStruct{ 
			string info;
			string warn;
			string error;
			string debug;
		} type {
			"INFO",
			"WARN",
			"ERROR",
			"DEBUG",
		};
		logStruct log;
		Log(string file, int level);
		~Log();
		bool write(string message, int level, string type);
};

#endif // LOG_HPP