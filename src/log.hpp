#ifndef LOG_HPP
#define LOG_HPP

#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>

#include "config.hpp"

class Log
{

private:
		
	const Config config_;
	bool logFileClosed;

	/*
	* Copy constructor private to prevent copy
	*/
	Log(const Log&);

	/*
	* Assignment operator= private to prevent assign
	*/
	Log& operator=(const Log&);
	
public:

	enum Level
	{
		INFO,
		DEBUG
	};

	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	Log(const Config& config):
	config_(config)
	{
		std::ofstream logFile_(config_.log_.file, logFile_.out | logFile_.app);
		/*
		* Checks if the log file is there or not
		*/
		if (! logFile_.is_open())
			std::cerr << "Error openning log file : " << config_.log_.file  << " make sure the directory and file exists " << "\n";

		/*
		* Checks and validates the level defined in the config.yaml
		* Also initialize the Log::Level
		*/
		if (config_.log_.level == std::string("INFO"))
			level_ = Level::INFO;
		else if (config_.log_.level == std::string("DEBUG"))
			level_ = Level::DEBUG;
		else
			std::cerr << "Invalid log level : " << config_.log_.level  << ", It should be one of [INFO|DEBUG] " << "\n";
		logFile_.close();
	}

	/*
	* Default distructor
	*/
	~Log(){}
	
	Level level_ = Level::INFO;

	/*
	*	This fucntion returns string of Log::Level
	*/
	const std::string levelToString(const Level level);

	/*
	*	This function writes message in to the log file
	*/
	void write(const std::string message, const Level level);
	
};

#endif /* LOG_HPP */