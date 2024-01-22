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
	if (streambufToString(readBuffer_) == "CLIENT HELLO\r\n\r\n")
	{
		std::iostream os(&writeBuffer_);
		std::string message("SERVER HELLO\r\n\r\n");
		os << message;
	} else
	{
		Request::pointer request_ = Request::create(config_, log_, readBuffer_);
		request_->detectType();
		log_->write("[ServerHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		boost::asio::io_context io_context_;
		TCPClient::pointer client_ = TCPClient::create(io_context_, config_, log_);
		client_->doConnect(request_->dstIP(), request_->dstPort());
		client_->writeBuffer(readBuffer_);
		client_->doWrite();
		client_->doRead();
	}
}