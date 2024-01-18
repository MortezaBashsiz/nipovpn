#include "serverhandler.hpp"

ServerHandler::ServerHandler(boost::asio::streambuf& readBuffer,
	boost::asio::streambuf& writeBuffer,
	const std::shared_ptr<Config>& config,
	const std::shared_ptr<Log>& log)
	:
		config_(config),
		log_(log),
		readBuffer_(readBuffer),
		writeBuffer_(writeBuffer)
{	}

ServerHandler::~ServerHandler()
{	}

void ServerHandler::handle()
{
	FUCK("ServerHandler::handle : "+streambufToString(readBuffer_));
	std::iostream os(&writeBuffer_);
	std::string message("OK");
	os << message;
}