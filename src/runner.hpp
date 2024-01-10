#ifndef RUNNER_HPP
#define RUNNER_HPP

/*
* This class is responsible to run the program and handle the exceptions
* It is called in main function(see main.cpp)
*/
class Runner : private Uncopyable
{
public:
	explicit Runner(boost::asio::io_context& io_context, const Config& config, const Log& log)
		:
			config_(config),
			io_context_(io_context),
			log_(log)
	{	}

	~Runner()
	{}

	/*
	* It is called in main function(see main.cpp) and will run the io_context
	*/
	void run()
	{
		try
		{
			log_.write("Config initialized in " + config_.modeToString() + " mode ", Log::Level::INFO);
			log_.write(config_.toString(), Log::Level::DEBUG);
			if (config_.runMode() == RunMode::server)
			{
				ServerTCPServer tcpServer_(io_context_, config_, log_);
				io_context_.run();
			}
			else if (config_.runMode() == RunMode::agent)
			{
				AgentTCPServer tcpAgent_(io_context_, config_, log_);
				io_context_.run();
			}
		}
		catch (std::exception& error)
		{
			log_.write(error.what(), Log::Level::ERROR);
			std::cerr << error.what() << std::endl;
		}
	}

private:
	const Config& config_;
	const Log& log_;
	boost::asio::io_context& io_context_;
	
};

#endif /* RUNNER_HPP */