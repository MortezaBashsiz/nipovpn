#include "runner.hpp"

Runner::Runner(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
		:
			config_(config),
			log_(log),
			io_context_(io_context)
	{	}

	Runner::~Runner()
	{}

void Runner::run()
{
	try
	{
		log_->write("Config initialized in " + config_->modeToString() + " mode ", Log::Level::INFO);
		log_->write(config_->toString(), Log::Level::DEBUG);
		TCPServer::pointer tcpServer_ = TCPServer::create(io_context_, config_, log_);
		io_context_.run();
	}
	catch (std::exception& error)
	{
		log_->write(error.what(), Log::Level::ERROR);
		std::cerr << error.what() << std::endl;
	}
}