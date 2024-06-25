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
#include "http.hpp"

/*
* Thic class is to create and handle TCP client
* Connects to the endpoint and handles the connection
*/
class TCPClient
  : public boost::enable_shared_from_this<TCPClient>
{
public:
	using pointer =  boost::shared_ptr<TCPClient>;

	static pointer create(boost::asio::io_context& io_context, 
		const std::shared_ptr<Config>& config, 
		const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPClient(io_context, config, log));
	}

	boost::asio::ip::tcp::socket& socket();
	void writeBuffer(boost::asio::streambuf& buffer);

	inline boost::asio::streambuf& writeBuffer()&
	{
		return writeBuffer_;
	}
	inline boost::asio::streambuf&& writeBuffer()&&
	{
		return std::move(writeBuffer_);
	}

	inline boost::asio::streambuf& readBuffer()&
	{
		return readBuffer_;
	}
	inline boost::asio::streambuf&& readBuffer()&&
	{
		return std::move(readBuffer_);
	}

	void doConnect();
	void doConnect(const std::string& dstIP, const unsigned short& dstPort);
	void doWrite(const HTTP::HttpType& httpType, 
		boost::asio::streambuf& buffer);
	void doRead();
	void doRead(const unsigned short& bytes, boost::asio::streambuf& buffer);
	void handleRead(const boost::system::error_code& error,
		size_t bytes_transferred);
	void doReadUntil(const std::string& until);
	void doReadSSL();

private:
		explicit TCPClient(boost::asio::io_context& io_context, 
			const std::shared_ptr<Config>& config, 
			const std::shared_ptr<Log>& log);

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::socket socket_;
	boost::asio::streambuf writeBuffer_, readBuffer_;
	boost::asio::ip::tcp::resolver resolver_;
};

#endif /* TCPCLIENT_HPP */