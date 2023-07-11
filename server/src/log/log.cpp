#include "log.h"

Log::Log(string file, int level){
	log.file	= file;
	log.level= level;
};

Log::~Log(){

};

bool Log::write(string message, int level, string type){
	if (level <= log.level){
		time_t now = time(0);
		auto time = std::time(nullptr);
		auto localtime = *std::localtime(&time);
		ostringstream oss;
		oss << std::put_time(&localtime, "%Y-%m-%d_%H:%M:%S");
		auto timeStr = oss.str();
		string line = timeStr + ", [" + type + "], " + message + "\n";

		ofstream file(log.file,  file.out | file.app);
		file.exceptions ( std::ifstream::badbit );
		if (file.is_open()){
			file << line;
			cout << line;
			file.close();
			return true;
		} else {
			cerr << "[ERROR]: Exception openning log file : " << log.file << "\n";
			return false;
		}
	}
  return true;
};