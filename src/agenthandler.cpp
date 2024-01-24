#include "agenthandler.hpp"

AgentHandler::AgentHandler(boost::asio::streambuf& readBuffer,
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

AgentHandler::~AgentHandler()
{	}

void AgentHandler::handle()
{
	Request::pointer request_ = Request::create(config_, log_, readBuffer_);
	if (request_->detectType())
	{
		log_->write("[AgentHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		client_->doConnect();
		client_->writeBuffer(readBuffer_);
		client_->doWrite();
		client_->doRead();
		copyStreamBuff(client_->readBuffer(), writeBuffer_);
	} else
	{
		log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::ERROR);
	}
}