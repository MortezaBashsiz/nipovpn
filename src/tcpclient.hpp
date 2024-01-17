#pragma once
#ifndef TCPClient_HPP
#define TCPClient_HPP

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>

#include "config.hpp"
#include "log.hpp"
#include "general.hpp"

/*
*	Thic class is to create and handle TCP connection
* First we will create a TCPServer and then in each accept action one connection will be created
*/
class TCPClientConnection
	: public boost::enable_shared_from_this<TCPClientConnection>
{
public:
	typedef boost::shared_ptr<TCPClientConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPClientConnection(io_context, config, log));
	}

	boost::asio::ip::tcp::socket& socket();

	void writeBuffer(const boost::asio::streambuf& buffer);

	const boost::asio::streambuf& readBuffer() const;

	void doRead();

	void handleRead(const boost::system::error_code& error,
		size_t bytes_transferred);

	void doWrite();

	void handleWrite(const boost::system::error_code& error,
		size_t bytes_transferred);
	
private:
	explicit TCPClientConnection(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log);
	
	boost::asio::ip::tcp::socket socket_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf readBuffer_, writeBuffer_;
};


/*
* Thic class is to create and handle TCP client
* Connects to the endpoint and handles the connection
*/
class TCPClient
{
public:
	typedef boost::shared_ptr<TCPClient> pointer;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPClient(io_context, config, log));
	}

	void doConnect();

	void doConnect(const std::string& ip, const unsigned short& port);

	void doWrite(const boost::asio::streambuf& wrtiteBuff, boost::asio::streambuf& readBuff);

private:
		explicit TCPClient(boost::asio::io_context& io_context, 
			const std::shared_ptr<Config>& config, 
			const std::shared_ptr<Log>& log);

	void handleConnect(TCPClientConnection::pointer newConnection,
		const boost::system::error_code& error);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::io_context& io_context_;
	TCPClientConnection::pointer connection_;
	boost::asio::ip::tcp::resolver resolver_;
};

#endif /* TCPClient_HPP */