#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

#include "request.hpp"
#include "response.hpp"

/*
* This class is the handler if the process is running in mode agent
*/
class AgentHandler : private Uncopyable
{
private:
	const Config& config_;
	Log log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;

public:
	AgentHandler(boost::asio::streambuf& readBuffer, boost::asio::streambuf& writeBuffer, const Config& config):
		config_(config),
		log_(config),
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
		Request request_(config_, readBuffer_);
		request_.detectType();
		if (request_.httpType_ == Request::HttpType::HTTPS)
		{
			FUCK("HTTPS");
			FUCK(request_.parsedTlsRequest_.sni);
		}
		else
		{
			FUCK("HTTP");
		}
	}
	
};

#endif /* AGENTHADLER_HPP */