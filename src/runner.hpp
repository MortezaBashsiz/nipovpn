#pragma once
#ifndef RUNNER_HPP
#define RUNNER_HPP

#include <iostream>

#include "config.hpp"
#include "log.hpp"
#include "tcpserver.hpp"

/*
* This class is responsible to run the program and handle the exceptions
* It is called in main function(see main.cpp)
*/
class Runner : private Uncopyable
{
public:

	explicit Runner(const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log);
	~Runner();

	/*
	* It is called in main function(see main.cpp) and will run the io_context
	*/
	void run();

private:
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;

};

#endif /* RUNNER_HPP */