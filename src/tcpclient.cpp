#include "tcpclient.hpp"

/*
* TCPClient
*/
TCPClient::TCPClient(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log)
	: config_(config),
		log_(log),
		io_context_(io_context),
		socket_(io_context),
		resolver_(io_context)
{ }

boost::asio::ip::tcp::socket& TCPClient::socket()
{
	return socket_;
}

void TCPClient::writeBuffer(const boost::asio::streambuf& buffer)
{
	copyStreamBuff(buffer, writeBuffer_);
}

const boost::asio::streambuf& TCPClient::readBuffer() const
{
	return readBuffer_;
}

void TCPClient::doConnect()
{
	log_->write("[TCPClient doConnect] [DST] " + 
		config_->agent().serverIp +":"+ 
		std::to_string(config_->agent().serverPort)+" ",
		Log::Level::DEBUG);

	boost::asio::ip::tcp::resolver resolver(io_context_);
	auto endpoint = resolver.resolve(config_->agent().serverIp.c_str(), std::to_string(config_->agent().serverPort).c_str());
	boost::asio::connect(socket_,
		endpoint
		);
}
void TCPClient::doConnect(const std::string& dstIP, const unsigned short& dstPort)
{
		log_->write("[TCPClient doConnect] [DST] " + 
		dstIP +":"+ 
		std::to_string(dstPort)+" ",
		Log::Level::DEBUG);

	boost::asio::ip::tcp::resolver resolver(io_context_);
	auto endpoint = resolver.resolve(dstIP.c_str(), std::to_string(dstPort).c_str());
	boost::asio::connect(socket_,
		endpoint
		);
}
void TCPClient::handleConnect(const boost::system::error_code& error)
{
	boost::asio::socket_base::keep_alive option(true);
	socket_.set_option(option);
	if (!error)
	{
		try
		{
			log_->write("[TCPClient handleConnect] [DST " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] ",
				Log::Level::DEBUG);
		}
		catch (std::exception& error)
		{
			log_->write(error.what(), Log::Level::ERROR);
		}
	} else
	{
		log_->write("[TCPClient handleConnect] " + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doWrite()
{
	try
	{
		log_->write("[TCPClient doWrite] [DST " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] "+
			"[Bytes " +
			std::to_string(writeBuffer_.size())+"] ",
			Log::Level::DEBUG);
		boost::asio::write(
			socket_,
			writeBuffer_
		);
	}
	catch (std::exception& error)
	{
		log_->write(error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doRead()
{
	boost::asio::async_read_until(
		socket_,
		readBuffer_,
		"\r\n\r\n",
		boost::bind(&TCPClient::handleRead, 
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPClient::handleRead(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::eof)
	{
		try
		{
			log_->write("[TCPClient doRead] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
				std::to_string(bytes_transferred)+"] ",
				Log::Level::INFO);
		}
		catch (std::exception& error)
		{
			log_->write(error.what(), Log::Level::ERROR);
		}
	} else
	{
		log_->write("[handleRead] " + error.what(), Log::Level::ERROR);
	}
}