#include "log.hpp"

Log::Log(Config nipoConfig){
	log.file	= nipoConfig.config.logFile;
	log.level = nipoConfig.config.logLevel;
};

Log::~Log(){

};

string Log::logLevelToString(int level) {
	switch (level){
		case levelInfo:
			return type.info;
		case levelWarn:
			return type.warn;
		case levelError:
			return type.error;
		case levelDebug:
			return type.debug;
		default:
			return type.info;
	};
};

void Log::write(string message, int level){
	message.erase(std::remove_if( 
		message.begin(),
  	message.end(),
  		[](auto ch)
  		{
  		    return (ch == '\n' ||
  		            ch == '\r'); 
  		}),
  	message.end() );
	if (level <= log.level){
		time_t now = time(0);
		auto time = std::time(nullptr);
		auto localtime = *std::localtime(&time);
		ostringstream oss;
		oss << std::put_time(&localtime, "%Y-%m-%d_%H:%M:%S");
		auto timeStr = oss.str();
		string line = timeStr + ", [" + logLevelToString(level) + "], " + message + "\n";
		ofstream file(log.file,  file.out | file.app);
		file.exceptions ( std::ifstream::badbit );
		if (file.is_open()){
			file << line;
			cout << line;
			file.close();
		} else {
			cerr << "[ERROR]: Exception openning log file : " << log.file << "\n";
			exit(1);
		}
	}
};