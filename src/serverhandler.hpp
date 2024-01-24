#pragma once
#ifndef SERVERHADLER_HPP
#define SERVERHADLER_HPP

#include <memory>

#include "config.hpp"
#include "log.hpp"
#include "request.hpp"
#include "general.hpp"
#include "tcpclient.hpp"

/*
* This class is the handler if the process is running in mode server
*/
class ServerHandler 
	: private Uncopyable
{
public:
	typedef std::shared_ptr<ServerHandler> pointer;

	static pointer create(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client)
	{
		return pointer(new ServerHandler(readBuffer, writeBuffer, config, log, client));
	}

	~ServerHandler();
	
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
	explicit ServerHandler(boost::asio::streambuf& readBuffer,
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

#endif /* SERVERHADLER_HPP */