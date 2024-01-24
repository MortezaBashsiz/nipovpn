#include "tcpconnection.hpp"

/*
* TCPConnection
*/
TCPConnection::TCPConnection(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log,
	const TCPClient::pointer& client)
	: socket_(io_context),
		config_(config),
		log_(log),
		client_(client)
{ }

boost::asio::ip::tcp::socket& TCPConnection::socket()
{
	return socket_;
}

void TCPConnection::writeBuffer(boost::asio::streambuf& buffer)
{
	copyStreamBuff(buffer, writeBuffer_);
}

const boost::asio::streambuf& TCPConnection::readBuffer() const
{
	return readBuffer_;
}

void TCPConnection::start()
{
	doRead();
}

void TCPConnection::doRead()
{
	boost::asio::async_read_until(
		socket_,
		readBuffer_,
		"\r\n",
		boost::bind(&TCPConnection::handleRead, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPConnection::handleRead(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::eof)
	{
		try
		{
			log_->write("[TCPConnection doRead] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
				std::to_string(bytes_transferred)+"] ",
				Log::Level::INFO);
		}
		catch (std::exception& error)
		{
			log_->write(std::string("[TCPConnection handleRead log] ") + error.what(), Log::Level::ERROR);
		}
		if (config_->runMode() == RunMode::agent)
		{
			AgentHandler::pointer agentHandler_ = AgentHandler::create(readBuffer_, writeBuffer_, config_, log_, client_);
			agentHandler_->handle();
		} else if (config_->runMode() == RunMode::server)
		{
			ServerHandler::pointer serverHandler_ = ServerHandler::create(readBuffer_, writeBuffer_, config_, log_,  client_);
			serverHandler_->handle();
		}
		doWrite();
	} else
	{
		log_->write(std::string("[TCPConnection handleRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPConnection::doWrite()
{
	try
	{
		log_->write("[TCPConnection doWrite] [DST " + 
			socket_.remote_endpoint().address().to_string() +":"+ 
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(writeBuffer_.size())+"] ", 
			Log::Level::DEBUG);
		boost::asio::write(
			socket_,
			writeBuffer_
		);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection doWrite] ") + error.what(), Log::Level::ERROR);
	}
}