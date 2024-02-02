#pragma once
#ifndef LOG_HPP
#define LOG_HPP

#include <ctime>
#include <iomanip>

#include "config.hpp"
#include "general.hpp"

class Log 
	: private Uncopyable
{
public:
	enum class Level
	{
		INFO,
		ERROR,
		DEBUG
	};

	using pointer =  std::shared_ptr<Log>;

	static pointer create(const std::shared_ptr<Config>& config)
	{
		return pointer(new Log(config));
	}

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Log(const std::shared_ptr<Log>& log);

	/*
	* Default distructor
	*/
	~Log();

	/*
	* Functions to handle private members
	*/
	inline const std::shared_ptr<Config>& config() const
	{
		return config_;
	}
	
	inline const Log::Level& level() const
	{
		return level_;
	}
	
	/*
	*	This fucntion returns string of Log::Level
	*/
	inline const std::string levelToString(const Level& level) const
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

	/*
	*	This function writes message in to the log file
	*/
	void write(const std::string& message, const Level& level) const;

private:

	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	explicit Log(const std::shared_ptr<Config>& config);

	const std::shared_ptr<Config>& config_;
	bool logFileClosed;
	Level level_ = Level::INFO;

};

#endif /* LOG_HPP */