#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

#include "config.hpp"
#include "log.hpp"
#include "request.hpp"
#include "general.hpp"

class TCPClient;

/*
* This class is the handler if the process is running in mode agent
*/
class AgentHandler 
	: private Uncopyable
{
public:
	typedef std::shared_ptr<AgentHandler> pointer;

	static pointer create(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
	{
		return pointer(new AgentHandler(readBuffer, writeBuffer, config, log));
	}

	~AgentHandler();

	/*
	*	This function is handling request. This function is called from TCPConnection::handleRead function(see tcp.hpp)
	* It creates a request object and proceeds to the next steps
	*/
	void handle();

private:
	AgentHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;
};

#endif /* AGENTHADLER_HPP */