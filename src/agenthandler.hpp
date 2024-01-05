#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

#include "request.hpp"
#include "response.hpp"

/*
* This class is the handler if the process is running in mode agent
*/
class AgentHandler : private Uncopyable
{
public:
	AgentHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const Config& config,
		const Log& log)
		:
			config_(config),
			log_(log),
			readBuffer_(readBuffer),
			writeBuffer_(writeBuffer)
	{	}

	~AgentHandler()
	{	}

	/*
	*	This function is handling request. This function is called from TCPConnection::handleRead function(see tcp.hpp)
	* It creates a request object and proceeds to the next steps
	*/
	void handle()
	{
		Request request_(config_, log_, readBuffer_);
		request_.detectType();
		log_.write("Request detail : "+request_.toString(), Log::Level::DEBUG);
		if (request_.httpType() == Request::HttpType::HTTPS)
		{

		}
		else
		{

		}
	}

private:
	const Config& config_;
	const Log& log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;
};

#endif /* AGENTHADLER_HPP */