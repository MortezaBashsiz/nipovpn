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
		writeBuffer_(writeBuffer),
		request_(Request::create(config, log, readBuffer))
{	}

AgentHandler::~AgentHandler()
{	}

void AgentHandler::handle()
{
	std::string newReq(request_->genHttpReqString(
				encode64(
					streambufToString(readBuffer_)
				)
			)
		);
	if (request_->detectType())
	{
		log_->write("[AgentHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		if (! client_->socket().is_open() || request_->httpType() == Request::HttpType::HTTP || request_->httpType() == Request::HttpType::CONNECT)
			client_->doConnect();
		copyStringToStreambuf(newReq, readBuffer_);
		client_->doWrite(request_->httpType(), request_->parsedHttpRequest().method(), readBuffer_);
		moveStreamBuff(client_->readBuffer(), writeBuffer_);
	} else
	{
		log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::ERROR);
	}
}