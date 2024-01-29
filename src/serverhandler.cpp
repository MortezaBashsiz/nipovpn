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
		writeBuffer_(writeBuffer),
		request_(Request::create(config, log, readBuffer))
{ }

ServerHandler::~ServerHandler()
{ }

void ServerHandler::handle()
{
	if (request_->detectType())
	{
		request_->detectType();
		log_->write("[ServerHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		if (!client_->socket().is_open())
			client_->doConnect(request_->dstIP(), request_->dstPort());
		if (request_->parsedHttpRequest().method() == boost::beast::http::verb::connect)
		{
			boost::asio::streambuf tempBuff;
			std::iostream os(&tempBuff);
			if (client_->socket().is_open())
			{
				std::string message("HTTP/1.1 200 Connection established\r\n\r\n");
				os << message;
			} else
			{
				std::string message("HTTP/1.1 500 Connection failed\r\n\r\n");
				os << message;
			}
			moveStreamBuff(tempBuff, writeBuffer_);
		} else
		{
			if (request_->httpType() == Request::HttpType::HTTPS && request_->parsedTlsRequest().step == Request::TlsSteps::ClientHello)
			{
				boost::asio::streambuf tempBuff;
				copyStringToStreambuf((streambufToString(readBuffer_) + "0000000000"), tempBuff);
				moveStreamBuff(tempBuff, readBuffer_);
			}
			client_->doWrite(request_->httpType(), request_->parsedHttpRequest().method(), readBuffer_);
			moveStreamBuff(client_->readBuffer(), writeBuffer_);
		}
	}else
	{
		log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::ERROR);
	}
}