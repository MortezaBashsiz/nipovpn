#pragma once
#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

#include <boost/beast/core/detail/base64.hpp>

#include <memory>

#include "config.hpp"
#include "log.hpp"
#include "request.hpp"
#include "general.hpp"
#include "tcpclient.hpp"

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
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client)
	{
		return pointer(new AgentHandler(readBuffer, writeBuffer, config, log, client));
	}

	~AgentHandler();

	/*
	*	This function is handling request. This function is called from TCPConnection::handleRead function(see tcp.hpp)
	* It creates a request object and proceeds to the next steps
	*/
	void handle();

	inline const Request::pointer request() const
	{
		return request_;
	}

private:
	AgentHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	const TCPClient::pointer& client_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;
	Request::pointer request_;
};

#endif /* AGENTHADLER_HPP */