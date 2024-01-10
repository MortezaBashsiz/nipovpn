#ifndef AGENT_HPP
#define AGENT_HPP

#include "agenthandler.hpp"

/*
*	Thic class is to create and handle TCP connection
* First we will create a AgentTCPServer and then in each accept action one connection will be created
*/
class AgentTCPConnection
	: private Uncopyable,
		public boost::enable_shared_from_this<AgentTCPConnection>
{
public:
	typedef boost::shared_ptr<AgentTCPConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, const Config& config, const Log& log)
	{
		return pointer(new AgentTCPConnection(io_context, config, log));
	}

	boost::asio::ip::tcp::socket& socket()
	{
		return socket_;
	}

	void writeBuffer(const boost::asio::streambuf& buffer)
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
			boost::bind(&AgentTCPConnection::handleRead, shared_from_this(),
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
			log_.write(" [AgentTCPConnection handleRead] Buffer : \n" + streambufToString(readBuffer_) , Log::Level::DEBUG);
			
			AgentHandler agentHandler_(readBuffer_, writeBuffer_, config_, log_);
			agentHandler_.handle();
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
			boost::bind(&AgentTCPConnection::handleWrite, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
		doRead();
	}

	void doWrite(boost::asio::streambuf buff)
	{
		writeBuffer(buff);
		doWrite();
	}

	void handleWrite(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (!error || error == boost::asio::error::broken_pipe)
		{
			log_.write(" [AgentTCPConnection handleWrite] Buffer : \n" + streambufToString(writeBuffer_) , Log::Level::DEBUG);
		} else
		{
			log_.write(" [AgentTCPConnection handleWrite] " + error.what(), Log::Level::ERROR);
		}
	}
	
private:
	explicit AgentTCPConnection(boost::asio::io_context& io_context, const Config& config, const Log& log)
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
class AgnetTCPClient : private Uncopyable, public boost::enable_shared_from_this<AgnetTCPClient>
{
public:
	explicit AgnetTCPClient(boost::asio::io_context& io_context, const Config& config, const Log& log)
		: config_(config),
			log_(log),
			io_context_(io_context),
			connection_(AgentTCPConnection::create(io_context, config, log)),
			resolver_(io_context)
	{	}

	void doConnect()
	{
		log_.write("[AgnetTCPClient doConnect] [DST] " + 
				config_.agent().serverIp +":"+ 
				std::to_string(config_.agent().serverPort)+" "
					, Log::Level::DEBUG);
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(config_.agent().serverIp),
					config_.agent().serverPort
			);

		connection_->socket().async_connect(endpoint,
			boost::bind(&AgnetTCPClient::handleConnect, this, connection_,
					boost::asio::placeholders::error, &log_));
	}

	void doConnect(const std::string& ip, const unsigned short& port)
	{
		log_.write("[AgnetTCPClient doConnect] [DST] " + 
				ip +":"+ std::to_string(port)+" "
					, Log::Level::DEBUG);
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip),	port);

		connection_->socket().async_connect(endpoint,
			boost::bind(&AgnetTCPClient::handleConnect, this, connection_,
					boost::asio::placeholders::error, &log_));
	}

	const boost::asio::streambuf& doWrite(const boost::asio::streambuf& buff)
	{
		connection_->writeBuffer(buff);
		connection_->doWrite();
		return connection_->readBuffer();
	}

private:
	void handleConnect(AgentTCPConnection::pointer newConnection,
		const boost::system::error_code& error, const Log* log)
	{
		if (!error)
		{
			log->write("[AgnetTCPClient handleConnect] [DST] " + 
				newConnection->socket().remote_endpoint().address().to_string() +":"+ 
				std::to_string(newConnection->socket().remote_endpoint().port())+" "
					, Log::Level::DEBUG);
		} else
		{
			log->write("[AgnetTCPClient handleConnect] " + error.what(), Log::Level::ERROR);
		}
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::resolver resolver_;
	AgentTCPConnection::pointer connection_;
	const Config& config_;
	const Log& log_;
};


/*
*	Thic class is to create and handle TCP server
* Listening to the IP:Port and handling the socket is here
*/
class AgentTCPServer : private Uncopyable
{
public:
	explicit AgentTCPServer(boost::asio::io_context& io_context, const Config& config, const Log& log)
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
		AgentTCPConnection::pointer newConnection =
			AgentTCPConnection::create(io_context_, config_, log_);

		acceptor_.async_accept(newConnection->socket(),
				boost::bind(&AgentTCPServer::handleAccept, this, newConnection,
					boost::asio::placeholders::error));
	}

	void handleAccept(AgentTCPConnection::pointer newConnection,
		const boost::system::error_code& error)
	{
		if (!error)
		{
			newConnection->listen();
			if (config_.runMode() == RunMode::agent)
			{
				AgnetTCPClient client(io_context_, config_, log_);
				client.doConnect();
			}
		}
		startAccept();
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	const Config& config_;
	const Log& log_;
};

#endif /* AGENT_HPP */