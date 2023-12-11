#include <iostream>

#include "config.hpp"
#include "log.hpp"
#include "nipo.hpp"

int main(int argc, char const *argv[])
{
	/*
	* Declare and initialize BoolStr item to validate the config
	*/
	BoolStr configValidation{false, std::string("FAILED")};

	/*
	* Call validate configuration with argc and argv
	* Check if the configuration is valid or not
	*/
	configValidation = validateConfig(argc, argv);
	if (! configValidation.ok)
	{
		std::cerr << configValidation.message;
		return 1;
	}

	/*
	* Declare and initialize main Config object
	* See config.hpp
	*/
	Config config_(runMode_, std::string(argv[2]));

	/*
	* Declare and initialize main Log object
	*/
	Log log_(config_);

	return 0;
}