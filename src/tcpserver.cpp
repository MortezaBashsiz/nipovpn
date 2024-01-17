#include "tcpserver.hpp"

/*
* TCPServerConnection
*/
TCPServerConnection::TCPServerConnection(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log)
	: socket_(io_context),
		config_(config),
		log_(log)
{ }

boost::asio::ip::tcp::socket& TCPServerConnection::socket()
{
	return socket_;
}

void TCPServerConnection::writeBuffer(const boost::asio::streambuf& buffer)
{
	copyStreamBuff(buffer, writeBuffer_);
}

const boost::asio::streambuf& TCPServerConnection::readBuffer() const
{
	return readBuffer_;
}

void TCPServerConnection::listen()
{
	doRead();
}

void TCPServerConnection::doRead()
{
	boost::asio::async_read_until(
		socket_,
		readBuffer_,
		"\r\n\r\n",
		boost::bind(&TCPServerConnection::handleRead, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPServerConnection::handleRead(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::eof)
	{
		log_->write("["+config_->modeToString()+"] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
			std::to_string(bytes_transferred)+"] ",
			Log::Level::INFO);		
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
		log_->write("[handleRead] " + error.what(), Log::Level::ERROR);
	}
}

void TCPServerConnection::doWrite()
{
	boost::asio::async_write(
		socket_,
		writeBuffer_,
		boost::bind(&TCPServerConnection::handleWrite, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPServerConnection::handleWrite(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error)
	{
		log_->write("[TCPServerConnection handleWrite] Buffer : \n" + streambufToString(writeBuffer_) +" "+
			std::to_string(bytes_transferred)+" ", 
			Log::Level::DEBUG);
	} else
	{
		log_->write("[TCPServerConnection handleWrite] " + error.what(), Log::Level::ERROR);
	}
}

/*
* TCPServer
*/
TCPServer::TCPServer(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log)
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

void TCPServer::startAccept()
{
	TCPServerConnection::pointer newConnection =
		TCPServerConnection::create(io_context_, config_, log_);
	
	acceptor_.async_accept(newConnection->socket(),
		boost::bind(&TCPServer::handleAccept, 
			this, 
			newConnection,
			boost::asio::placeholders::error)
		);
}

void TCPServer::handleAccept(TCPServerConnection::pointer newConnection,
	const boost::system::error_code& error)
{
	if (!error)
	{
		newConnection->listen();
	}
	startAccept();
}