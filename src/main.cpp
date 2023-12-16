#include <iostream>
#include <boost/asio.hpp>

#include "config.hpp"
#include "log.hpp"
#include "socket.hpp"

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

	log_.write("Config initialized", Log::Level::INFO);
	log_.write(config_.toString(), Log::Level::DEBUG);
	
	try
	{
		boost::asio::io_context io_context;
		TCPServer tcpServer_(io_context, config_);
		io_context.run();
	}
	catch (std::exception& error)
	{
		log_.write(error.what(), Log::Level::ERROR);
		std::cerr << error.what() << std::endl;
	}

	return 0;
}