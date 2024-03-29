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
	using pointer =  boost::shared_ptr<TCPConnection>;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client)
	{
		return pointer(new TCPConnection(io_context, config, log, client));
	}

	boost::asio::ip::tcp::socket& socket();

	inline void writeBuffer(boost::asio::streambuf& buffer)
	{
		moveStreamBuff(buffer, writeBuffer_);
	}
	
	inline const boost::asio::streambuf& readBuffer()&
	{
		return readBuffer_;
	}
	inline const boost::asio::streambuf&& readBuffer()&&
	{
		return std::move(readBuffer_);
	}

	void start();

	void doRead(const unsigned short& bytes, boost::asio::streambuf& buffer);

	void doReadUntil(const std::string& until);

	void doReadSSL();

	void handleRead(const boost::system::error_code& error,
		size_t bytes_transferred);
	void handleReadSSL(const boost::system::error_code& error);

	void doWrite();
	
private:
	explicit TCPConnection(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log,
		const TCPClient::pointer& client);
	
	boost::asio::ip::tcp::socket socket_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::io_context& io_context_;
	const TCPClient::pointer& client_;
	boost::asio::streambuf readBuffer_, writeBuffer_;
};

#endif /* TCPCONNECTION_HPP */