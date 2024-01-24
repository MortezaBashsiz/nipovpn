#include "serverhandler.hpp"

ServerHandler::ServerHandler(boost::asio::streambuf& readBuffer,
	boost::asio::streambuf& writeBuffer,
	const std::shared_ptr<Config>& config,
	const std::shared_ptr<Log>& log,
	const TCPClient::pointer& client)
	:
		config_(config),
		log_(log),
		client_(client),
		readBuffer_(readBuffer),
		writeBuffer_(writeBuffer)
{	}

ServerHandler::~ServerHandler()
{	}

void ServerHandler::handle()
{
	Request::pointer request_ = Request::create(config_, log_, readBuffer_);
	if (request_->detectType())
	{
		request_->detectType();
		log_->write("[ServerHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		client_->doConnect(request_->dstIP(), request_->dstPort());
		if (request_->parsedHttpRequest().method() == boost::beast::http::verb::connect && client_->socket().is_open())
		{
			boost::asio::streambuf tempBuff;
			std::iostream os(&tempBuff);
			std::string message("HTTP/1.1 200 Connection established\r\n");
			os << message;
			copyStreamBuff(tempBuff, writeBuffer_);
		} else
		{
			client_->writeBuffer(readBuffer_);
			client_->doWrite();
			client_->doRead();
			copyStreamBuff(client_->readBuffer(), writeBuffer_);
		}
	}else
	{
		log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::ERROR);
	}
}