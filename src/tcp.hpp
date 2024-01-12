#ifndef TCP_HPP
#define TCP_HPP

/*
*	Thic class is to create and handle TCP connection
* First we will create a TCPServer and then in each accept action one connection will be created
*/
class TCPConnection
	: private Uncopyable,
		public boost::enable_shared_from_this<TCPConnection>
{
public:
	typedef boost::shared_ptr<TCPConnection> pointer;

	static pointer create(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPConnection(io_context, config, log));
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
			log_->write("["+config_->modeToString()+"], SRC " +
					socket_.remote_endpoint().address().to_string() +":"+std::to_string(socket_.remote_endpoint().port())+" "
					, Log::Level::INFO);
			log_->write(" [TCPConnection handleRead] Buffer : \n" + streambufToString(readBuffer_) , Log::Level::DEBUG);
			
			if (config_->runMode() == RunMode::agent)
			{
				AgentHandler::pointer agentHandler_ = AgentHandler::create(readBuffer_, writeBuffer_, config_, log_);
				agentHandler_->handle();
			} else if (config_->runMode() == RunMode::server)
			{
				ServerHandler::pointer serverHandler_ = ServerHandler::create(readBuffer_, writeBuffer_, config_, log_);
				serverHandler_->handle();
			}
			doWrite();
		} else
		{
			log_->write(" [handleRead] " + error.what(), Log::Level::ERROR);
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
			log_->write(" [TCPConnection handleWrite] Buffer : \n" + streambufToString(writeBuffer_) , Log::Level::DEBUG);
		} else
		{
			log_->write(" [TCPConnection handleWrite] " + error.what(), Log::Level::ERROR);
		}
	}
	
private:
	explicit TCPConnection(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
		: socket_(io_context),
			config_(config),
			log_(log)
	{ }

	boost::asio::ip::tcp::socket socket_;
	boost::asio::streambuf readBuffer_, writeBuffer_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
};


/*
* Thic class is to create and handle TCP client
* Connects to the endpoint and handles the connection
*/
class TCPClient 
	: private Uncopyable,
		public boost::enable_shared_from_this<TCPClient>
{
public:
	typedef std::shared_ptr<TCPClient> pointer;

	static pointer create(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPClient(io_context, config, log));
	}

	void doConnect()
	{
		log_->write("[TCPClient doConnect] [DST] " + 
				config_->agent().serverIp +":"+ 
				std::to_string(config_->agent().serverPort)+" "
					, Log::Level::DEBUG);
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(config_->agent().serverIp),
					config_->agent().serverPort
			);

		connection_->socket().async_connect(endpoint,
			boost::bind(&TCPClient::handleConnect, this, connection_,
					boost::asio::placeholders::error, log_));
	}

	void doConnect(const std::string& ip, const unsigned short& port)
	{
		log_->write("[TCPClient doConnect] [DST] " + 
				ip +":"+ std::to_string(port)+" "
					, Log::Level::DEBUG);
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip),	port);

		connection_->socket().async_connect(endpoint,
			boost::bind(&TCPClient::handleConnect, this, connection_,
					boost::asio::placeholders::error, log_));
	}

	void doWrite(const boost::asio::streambuf& wrtiteBuff, boost::asio::streambuf& readBuff)
	{
		connection_->writeBuffer(wrtiteBuff);
		connection_->doWrite();
		copyStreamBuff(connection_->readBuffer(), readBuff);
	}

private:
		explicit TCPClient(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
		: config_(config),
			log_(log),
			io_context_(io_context),
			connection_(TCPConnection::create(io_context, config, log)),
			resolver_(io_context)
	{	}

	void handleConnect(TCPConnection::pointer newConnection,
		const boost::system::error_code& error, const std::shared_ptr<Log>& log)
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
	TCPConnection::pointer connection_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
};


/*
*	Thic class is to create and handle TCP server
* Listening to the IP:Port and handling the socket is here
*/
class TCPServer 
	: private Uncopyable,
		public boost::enable_shared_from_this<TCPServer>
{
public:
	typedef std::shared_ptr<TCPServer> pointer;

	static pointer create(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
	{
		return pointer(new TCPServer(io_context, config, log));
	}

private:
	explicit TCPServer(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
		: config_(config),
			log_(log),
			io_context_(io_context),
			acceptor_(
				io_context,
				boost::asio::ip::tcp::endpoint(
					boost::asio::ip::address::from_string(config->listenIp()),
					config->listenPort()
				)
			)
	{
		startAccept();
	}

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
		}
		startAccept();
	}

	boost::asio::io_context& io_context_;
	boost::asio::ip::tcp::acceptor acceptor_;
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
};

#endif /* TCP_HPP */