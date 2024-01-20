#pragma once
#ifndef TCPCLIENT_HPP
#define TCPCLIENT_HPP

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind/bind.hpp>

#include "config.hpp"
#include "log.hpp"
#include "general.hpp"


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

	boost::asio::ip::tcp::socket& socket();
	void writeBuffer(const boost::asio::streambuf& buffer);
	const boost::asio::streambuf& readBuffer() const;


	void doConnect();
	void doWrite();
	void doRead();

private:
		explicit TCPClient(boost::asio::io_context& io_context, 
			const std::shared_ptr<Config>& config, 
			const std::shared_ptr<Log>& log);

	void handleConnect(const boost::system::error_code& error);
	void handleRead(const boost::system::error_code& error,	size_t bytes_transferred);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::socket socket_;
	boost::asio::streambuf writeBuffer_, readBuffer_;
	boost::asio::ip::tcp::resolver resolver_;
};

#endif /* TCPCLIENT_HPP */