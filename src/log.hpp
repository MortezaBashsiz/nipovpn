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
	enum Level
	{
		INFO,
		ERROR,
		DEBUG
	};

	typedef std::shared_ptr<Log> pointer;

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
	const std::shared_ptr<Config>& config() const;

	const Level& level() const;

	/*
	*	This fucntion returns string of Log::Level
	*/
	const std::string levelToString(const Level& level) const;

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