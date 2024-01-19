#pragma once
#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>

#include "serverhandler.hpp"
#include "agenthandler.hpp"
#include "general.hpp"
#include "tcpconnection.hpp"

/*
*	Thic class is to create and handle TCP server
* Listening to the IP:Port and handling the socket is here
*/
class TCPServer 
{
public:
	typedef boost::shared_ptr<TCPServer> pointer;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client)
	{
		return pointer(new TCPServer(io_context, config, log, client));
	}

private:
	explicit TCPServer(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client);

	void startAccept();

	void handleAccept(TCPConnection::pointer newConnection,
		const boost::system::error_code& error);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	const TCPClient::pointer& client_;
	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
};

#endif /* TCPSERVER_HPP */