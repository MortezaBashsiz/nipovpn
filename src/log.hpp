#ifndef LOG_HPP
#define LOG_HPP

#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "config.hpp"

using namespace std;

class Log{
	private:
		struct logStruct{
			string file;
			int level;
		};
	public:
		enum logLevels{
			levelInfo = 1,
			levelWarn = 2,
			levelError = 3,
			levelDebug = 4
		};
		const struct typeStruct{ 
			string info;
			string warn;
			string error;
			string debug;
		} type = {
			"INFO",
			"WARN",
			"ERROR",
			"DEBUG"
		};
		logStruct log;
		Log(Config nipoConfig);
		~Log();
		string logLevelToString(int level);
		void write(string message, int level);
};

#endif // LOG_HPP