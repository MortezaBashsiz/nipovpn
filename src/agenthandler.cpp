#include "agenthandler.hpp"

AgentHandler::AgentHandler(boost::asio::streambuf& readBuffer,
	boost::asio::streambuf& writeBuffer,
	const std::shared_ptr<Config>& config,
	const std::shared_ptr<Log>& log)
	:
		config_(config),
		log_(log),
		readBuffer_(readBuffer),
		writeBuffer_(writeBuffer)
{	}

AgentHandler::~AgentHandler()
{	}

void AgentHandler::handle()
{
	Request::pointer request_ = Request::create(config_, log_, readBuffer_);
	request_->detectType();
	log_->write("Request detail : "+request_->toString(), Log::Level::DEBUG);
	// boost::asio::io_context io_context_;
	// TCPClient:pointer tcpClient_ = TCPClient::create(io_context_, config_, log_);
	if (request_->httpType() == Request::HttpType::HTTPS)
	{
		copyStreamBuff(readBuffer_, writeBuffer_);
	}
	else
	{
		copyStreamBuff(readBuffer_, writeBuffer_);
	}
}

