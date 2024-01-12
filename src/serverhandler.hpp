#ifndef SERVERHADLER_HPP
#define SERVERHADLER_HPP

#include <memory>

#include "config.hpp"
#include "log.hpp"

class ServerHandler 
	: private Uncopyable
{
public:
	typedef std::shared_ptr<ServerHandler> pointer;

	static pointer create(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
	{
		return pointer(new ServerHandler(readBuffer, writeBuffer, config, log));
	}

	~ServerHandler()
	{	}

	void handle()
	{
		std::iostream os(&writeBuffer_);
		std::string message("OK");
		os << message;
	}

private:
	explicit ServerHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
		:
			config_(config),
			log_(log),
			readBuffer_(readBuffer),
			writeBuffer_(writeBuffer)
	{	}
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;

};

#endif /* SERVERHADLER_HPP */