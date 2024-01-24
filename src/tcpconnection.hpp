#pragma once
#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>

#include "serverhandler.hpp"
#include "agenthandler.hpp"
#include "general.hpp"

/*
*	Thic class is to create and handle TCP connection
* First we will create a TCPServer and then in each accept action one connection will be created
*/
class TCPConnection
	: public boost::enable_shared_from_this<TCPConnection>
{
public:
	typedef boost::shared_ptr<TCPConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client)
	{
		return pointer(new TCPConnection(io_context, config, log, client));
	}

	boost::asio::ip::tcp::socket& socket();

	void writeBuffer(boost::asio::streambuf& buffer);

	const boost::asio::streambuf& readBuffer() const;

	void start();

	void doRead();

	void doReadSSL();

	void handleRead(const boost::system::error_code& error,
		size_t bytes_transferred,
		bool isReadSSL);

	void doWrite();
	
private:
	explicit TCPConnection(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client);
	
	boost::asio::ip::tcp::socket socket_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	const TCPClient::pointer& client_;
	boost::asio::streambuf readBuffer_, writeBuffer_;
};

#endif /* TCPCONNECTION_HPP */