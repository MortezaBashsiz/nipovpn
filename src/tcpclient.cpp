#include "tcpclient.hpp"

/*
* TCPClientConnection
*/
TCPClientConnection::TCPClientConnection(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
	: socket_(io_context),
		config_(config),
		log_(log)
{ }

boost::asio::ip::tcp::socket& TCPClientConnection::socket()
{
	return socket_;
}

void TCPClientConnection::writeBuffer(const boost::asio::streambuf& buffer)
{
	copyStreamBuff(buffer, writeBuffer_);
}

const boost::asio::streambuf& TCPClientConnection::readBuffer() const
{
	return readBuffer_;
}

void TCPClientConnection::doRead()
{
	boost::asio::async_read(
		socket_,
		readBuffer_,
		boost::bind(&TCPClientConnection::handleRead, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPClientConnection::handleRead(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::eof)
	{
		log_->write("["+config_->modeToString()+"], SRC " +
				socket_.remote_endpoint().address().to_string() +":"+std::to_string(socket_.remote_endpoint().port())+" "+
				std::to_string(bytes_transferred)+" "
				, Log::Level::INFO);
		log_->write("[TCPClientConnection handleRead] Buffer : \n" + streambufToString(readBuffer_) , Log::Level::DEBUG);
	} else
	{
		log_->write("[handleRead] " + error.what(), Log::Level::ERROR);
	}
}

void TCPClientConnection::doWrite()
{
	boost::asio::async_write(
		socket_,
		writeBuffer_,
		boost::bind(&TCPClientConnection::handleWrite, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPClientConnection::handleWrite(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::broken_pipe)
	{
		log_->write("[TCPClientConnection handleWrite] Buffer : \n" + streambufToString(writeBuffer_) +" "+
				std::to_string(bytes_transferred)+" ", 
				Log::Level::DEBUG);
		doRead();
	} else
	{
		log_->write("[TCPClientConnection handleWrite] " + error.what(), Log::Level::ERROR);
	}
}

/*
* TCPClient
*/
TCPClient::TCPClient(boost::asio::io_context& io_context, const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log)
	: config_(config),
		log_(log),
		io_context_(io_context),
		connection_(TCPClientConnection::create(io_context, config, log)),
		resolver_(io_context)
{	}

void TCPClient::doConnect()
{
	log_->write("[TCPClient doConnect] [DST] " + 
		config_->agent().serverIp +":"+ 
		std::to_string(config_->agent().serverPort)+" ",
		Log::Level::DEBUG);

	boost::asio::ip::tcp::resolver resolver(io_context_);
	auto endpoint = resolver.resolve(config_->agent().serverIp.c_str(), std::to_string(config_->agent().serverPort).c_str());
	
	boost::asio::async_connect(connection_->socket(),
		endpoint,
		boost::bind(&TCPClient::handleConnect, 
			shared_from_this(), 
			connection_,
			boost::asio::placeholders::error)
		);
}

void TCPClient::doConnect(const std::string& ip, const unsigned short& port)
{
	log_->write("[TCPClient doConnect] [DST] " + 
		ip +":"+ std::to_string(port)+" ",
		Log::Level::DEBUG);

	boost::asio::ip::tcp::resolver resolver(io_context_);
	auto endpoint = resolver.resolve(ip.c_str(), std::to_string(port).c_str());
	
	boost::asio::async_connect(connection_->socket(),
		endpoint,
		boost::bind(&TCPClient::handleConnect, 
			shared_from_this(), 
			connection_,
			boost::asio::placeholders::error)
		);
}

void TCPClient::doWrite(const boost::asio::streambuf& wrtiteBuff, boost::asio::streambuf& readBuff)
{
	connection_->writeBuffer(wrtiteBuff);
	connection_->doWrite();
	copyStreamBuff(connection_->readBuffer(), readBuff);
}

void TCPClient::handleConnect(TCPClientConnection::pointer newConnection,
	const boost::system::error_code& error)
{
	FUCK("handleConnect");
	if (!error)
	{
		log_->write("[TCPClient handleConnect] [DST] " + 
			newConnection->socket().remote_endpoint().address().to_string() +":"+ 
			std::to_string(newConnection->socket().remote_endpoint().port())+" ",
			Log::Level::DEBUG);
	} else
	{
		log_->write("[TCPClient handleConnect] " + error.what(), Log::Level::ERROR);
	}
}