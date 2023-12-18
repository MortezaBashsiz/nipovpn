#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <iostream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

class TCPConnection
	: public boost::enable_shared_from_this<TCPConnection>
{
public:
	typedef boost::shared_ptr<TCPConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, const Config& config)
	{
		return pointer(new TCPConnection(io_context, config));
	}

	boost::asio::ip::tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		doRead();
	}

	void doRead()
	{
		boost::asio::async_read(
				socket_,
				buffer_,
				boost::bind(&TCPConnection::handleRead, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
	}

	void handleRead(const boost::system::error_code& error,
			size_t bytes_transferred)
	{
		if (!error || error == boost::asio::error::eof)
		{
			std::string result(boost::asio::buffers_begin(buffer_.data()), boost::asio::buffers_begin(buffer_.data()) + buffer_.size());
			log_.write(result, Log::Level::DEBUG);
			doWrite();
		} else
		{
			log_.write(error.what(), Log::Level::ERROR);
		}
	}

	void doWrite()
	{
		boost::asio::async_write(
				socket_,
				buffer_,
				boost::bind(&TCPConnection::handleWrite, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
	}

	void handleWrite(const boost::system::error_code& error,
			size_t bytes_transferred)
	{
		if (!error)
		{
			FUCK(bytes_transferred);
		} else
		{
			log_.write(error.what(), Log::Level::ERROR);
		}
	}

private:
	TCPConnection(boost::asio::io_context& io_context, const Config& config)
		: socket_(io_context),
			config_(config),
			log_(config)
	{
	}

	boost::asio::ip::tcp::socket socket_;
	boost::asio::streambuf buffer_;
	const Config& config_;
	Log log_;
};

class TCPServer : private Uncopyable
{
public:
	TCPServer(boost::asio::io_context& io_context, const Config& config)
		: config_(config),
			log_(config),
			io_context_(io_context),
			acceptor_(
									io_context, 
									boost::asio::ip::tcp::endpoint(
										boost::asio::ip::address::from_string(config.listenIp_), 
										config.listenPort_
									)
								)
	{
		startAccept();
	}

private:
	void startAccept()
	{
		TCPConnection::pointer newConnection =
			TCPConnection::create(io_context_, config_);

		acceptor_.async_accept(newConnection->socket(),
				boost::bind(&TCPServer::handleAccept, this, newConnection,
					boost::asio::placeholders::error));
	}

	void handleAccept(TCPConnection::pointer newConnection,
			const boost::system::error_code& error)
	{
		if (!error)
		{
			newConnection->start();
		}
		startAccept();
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	const Config& config_;
	Log log_;
};

#endif /* TCPSERVER_HPP */