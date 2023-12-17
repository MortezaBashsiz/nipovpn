#ifndef SERVER_HPP
#define SERVER_HPP

class Server
{
private:
	Config& config_;
	TCPServer tcpServer_;
	boost::asio::io_context& io_context_;
	Log log_;

public:
	Server(boost::asio::io_context& io_context, Config& config):
		config_(config),
		io_context_(io_context),
		tcpServer_(io_context, config),
		log_(config)
	{
	}

	~Server()
	{}

	void run()
	{
		try
		{
			log_.write("Config initialized in " + config_.modeToString() + " mode ", Log::Level::INFO);
			log_.write(config_.toString(), Log::Level::DEBUG);
			io_context_.run();
		}
		catch (std::exception& error)
		{
			log_.write(error.what(), Log::Level::ERROR);
			std::cerr << error.what() << std::endl;
		}
	}
	
};

#endif /* SERVER_HPP */