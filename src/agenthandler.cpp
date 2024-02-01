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
		request_(HTTP::create(config, log, readBuffer))
{	}

AgentHandler::~AgentHandler()
{	}

void AgentHandler::handle()
{
	std::string newReq(request_->genHttpPostReqString(
				encode64(
					streambufToString(readBuffer_)
				)
			)
		);
	if (request_->detectType())
	{
		log_->write("[AgentHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
		if (! client_->socket().is_open() || request_->httpType() == HTTP::HttpType::http || request_->httpType() == HTTP::HttpType::connect)
			client_->doConnect();
		copyStringToStreambuf(newReq, readBuffer_);
		log_->write("[AgentHandler handle] [Request To Server] : \n"+newReq, Log::Level::DEBUG);
		client_->doWrite(request_->httpType(), request_->parsedHttpRequest().method(), readBuffer_);
		HTTP::pointer response = HTTP::create(config_, log_, client_->readBuffer());
		if (response->parseHttpResp())
		{
			log_->write("[AgentHandler handle] [Response] : "+response->restoString(), Log::Level::DEBUG);
			copyStringToStreambuf(decode64(boost::lexical_cast<std::string>(response->parsedHttpResponse().body())), writeBuffer_);
		}
		else
		{
			log_->write("[AgentHandler handle] [NOT HTTP Response] [Response] : "+ streambufToString(client_->readBuffer()), Log::Level::ERROR);
		}
	} else
	{
		log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::ERROR);
	}
}