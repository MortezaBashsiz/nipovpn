#include "log.hpp"

const std::string Log::levelToString(const Level level) 
{
	std::string result("");
	switch (level){
		case Level::INFO:
			result = "INFO";
			break;
		case Level::ERROR:
			result = "ERROR";
			break;
		case Level::DEBUG:
			result = "DEBUG";
			break;
	}
	return result;
}

void Log::write(const std::string message, const Level level)
{
	std::ofstream logFile_(config_.log_.file, logFile_.out | logFile_.app);
	if (level <= level_ || level == Level::ERROR){
		if (logFile_.is_open())
		{
			time_t now = time(0);
			auto time = std::time(nullptr);
			auto localtime = *std::localtime(&time);
			std::ostringstream oss;
			oss << std::put_time(&localtime, "%Y-%m-%d_%H:%M:%S");
			auto timeStr = oss.str();
			std::string line = timeStr + " [" + levelToString(level) + "] " + message + "\n";
			logFile_ << line;
			std::cout << line;
			logFile_.close();
		} else 
		{
			std::cerr << "Error openning log file : " << config_.log_.file  << " make sure the directory and file exists " << "\n";
		}
	}
}