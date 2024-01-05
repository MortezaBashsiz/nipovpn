#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include "agenthandler.hpp"
#include "serverhandler.hpp"

/*
*	Thic class is to create and handle TCP connection
* First we will create a TCPServer and then in each accept action one connection will be created
*/
class TCPConnection
	: public boost::enable_shared_from_this<TCPConnection>
{
public:
	typedef boost::shared_ptr<TCPConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, const Config& config, const Log& log)
	{
		return pointer(new TCPConnection(io_context, config, log));
	}

	boost::asio::ip::tcp::socket& socket()
	{
		return socket_;
	}

	void writeBuffer(boost::asio::streambuf& buffer)
	{
		copyStreamBuff(buffer, writeBuffer_);
	}

	const boost::asio::streambuf& readBuffer() const
	{
		return readBuffer_;
	}

	void listen()
	{
		doRead();
	}

	void doRead()
	{
		boost::asio::async_read(
			socket_,
			readBuffer_,
			boost::bind(&TCPConnection::handleRead, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
	}

	void handleRead(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (!error || error == boost::asio::error::eof)
		{
			log_.write("["+config_.modeToString()+"], SRC " +
					socket_.remote_endpoint().address().to_string() +":"+std::to_string(socket_.remote_endpoint().port())+" "
					, Log::Level::INFO);
			log_.write(" [TCPConnection handleRead] Buffer : \n" + streambufToString(readBuffer_) , Log::Level::DEBUG);
			if (config_.runMode() == RunMode::agent)
			{
				AgentHandler agentHandler_(readBuffer_, writeBuffer_, config_, log_);
				agentHandler_.handle();
			} else if (config_.runMode() == RunMode::server)
			{
				ServerHandler serverHandler_(readBuffer_, writeBuffer_, config_);
				serverHandler_.handle();
			}
		} else
		{
			log_.write(" [handleRead] " + error.what(), Log::Level::ERROR);
		}
	}

	void doWrite()
	{
		boost::asio::async_write(
			socket_,
			writeBuffer_,
			boost::bind(&TCPConnection::handleWrite, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
	}

	void handleWrite(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (!error || error == boost::asio::error::broken_pipe)
		{
			log_.write(" [TCPConnection handleWrite] Buffer : \n" + streambufToString(writeBuffer_) , Log::Level::DEBUG);
		} else
		{
			log_.write(" [TCPConnection handleWrite] " + error.what(), Log::Level::ERROR);
		}
	}
	
private:
	explicit TCPConnection(boost::asio::io_context& io_context, const Config& config, const Log& log)
		: socket_(io_context),
			config_(config),
			log_(log)
	{ }

	boost::asio::ip::tcp::socket socket_;
	boost::asio::streambuf readBuffer_, writeBuffer_;
	const Config& config_;
	const Log& log_;
};


/*
*	Thic class is to create and handle TCP client
* Connects to the endpoint and handles the connection
*/
class TCPClient : private Uncopyable
{
public:
	explicit TCPClient(boost::asio::io_context& io_context, const Config& config, const Log& log)
		: config_(config),
			log_(log),
			io_context_(io_context),
			resolver_(io_context)
	{	}

	void doConnect()
	{
		boost::asio::ip::tcp::endpoint endpoints(boost::asio::ip::address::from_string(config_.agent().serverIp),
					config_.agent().serverPort
			);

		TCPConnection::pointer newConnection =
			TCPConnection::create(io_context_, config_, log_);

		newConnection->socket().async_connect(endpoints,
			boost::bind(&TCPClient::handleConnect, this, newConnection,
					boost::asio::placeholders::error, &log_));
	}

private:
	void handleConnect(TCPConnection::pointer newConnection,
		const boost::system::error_code& error, const Log* log)
	{
		if (!error)
		{
			log->write("[TCPClient handleConnect] [DST] " + 
				newConnection->socket().remote_endpoint().address().to_string() +":"+ 
				std::to_string(newConnection->socket().remote_endpoint().port())+" "
					, Log::Level::DEBUG);
		} else
		{
			log->write("[TCPClient handleConnect] " + error.what(), Log::Level::ERROR);
		}
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::resolver resolver_;
	const Config& config_;
	const Log& log_;
};


/*
*	Thic class is to create and handle TCP server
* Listening to the IP:Port and handling the socket is here
*/
class TCPServer : private Uncopyable
{
public:
	explicit TCPServer(boost::asio::io_context& io_context, const Config& config, const Log& log)
		: config_(config),
			log_(log),
			io_context_(io_context),
			acceptor_(
				io_context,
				boost::asio::ip::tcp::endpoint(
					boost::asio::ip::address::from_string(config.listenIp()),
					config.listenPort()
				)
			)
	{
		startAccept();
	}

private:
	void startAccept()
	{
		TCPConnection::pointer newConnection =
			TCPConnection::create(io_context_, config_, log_);

		acceptor_.async_accept(newConnection->socket(),
				boost::bind(&TCPServer::handleAccept, this, newConnection,
					boost::asio::placeholders::error));
	}

	void handleAccept(TCPConnection::pointer newConnection,
		const boost::system::error_code& error)
	{
		if (!error)
		{
			newConnection->listen();
			if (config_.runMode() == RunMode::agent)
			{
				TCPClient client_(io_context_, config_, log_);
				client_.doConnect();
			}
		}
		startAccept();
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	const Config& config_;
	const Log& log_;
};

#endif /* TCPSERVER_HPP */