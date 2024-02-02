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

void TCPClient::writeBuffer(boost::asio::streambuf& buffer)
{
	moveStreamBuff(buffer, writeBuffer_);
}

void TCPClient::doConnect()
{
	try
	{
		log_->write("[TCPClient doConnect] [DST] " +
			config_->agent().serverIp +":"+
			std::to_string(config_->agent().serverPort)+" ",
			Log::Level::DEBUG);
	
		boost::asio::ip::tcp::resolver resolver(io_context_);
		auto endpoint = resolver.resolve(config_->agent().serverIp.c_str(), std::to_string(config_->agent().serverPort).c_str());
		boost::asio::connect(socket_, endpoint);
		boost::asio::socket_base::keep_alive option(true);
		socket_.set_option(option);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doConnect] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doConnect(const std::string& dstIP, const unsigned short& dstPort)
{
	try
	{
		log_->write("[TCPClient doConnect] [DST] " +
			dstIP +":"+
			std::to_string(dstPort)+" ",
			Log::Level::DEBUG);
	
		boost::asio::ip::tcp::resolver resolver(io_context_);
		auto endpoint = resolver.resolve(dstIP.c_str(), std::to_string(dstPort).c_str());
		boost::asio::connect(socket_, endpoint);
		boost::asio::socket_base::keep_alive option(true);
		socket_.set_option(option);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doConnect Custom] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doWrite(const HTTP::HttpType& httpType, 
  boost::asio::streambuf& buffer)
{
	try
	{
		moveStreamBuff(buffer, writeBuffer_);
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
		if (httpType == HTTP::HttpType::https)
			doReadSSL();
		else if (httpType == HTTP::HttpType::http)
			doRead();
		else if (httpType == HTTP::HttpType::connect)
			doReadUntil("\r\n\r\n");
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doWrite] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doRead()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::system::error_code error;
		boost::asio::read(
			socket_,
			readBuffer_,
			error
		);
		if (!error || error == boost::asio::error::eof)
		{
			log_->write("[TCPClient doRead] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
				std::to_string(readBuffer_.size())+"] ",
				Log::Level::DEBUG);
		} else
		{
			log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doReadUntil(const std::string& until)
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::asio::read_until(
			socket_,
			readBuffer_,
			until
		);
		log_->write("[TCPClient doReadUntil] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(readBuffer_.size())+"] ",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doReadSSL()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::system::error_code error;
		boost::asio::read(
			socket_,
			readBuffer_,
			boost::asio::transfer_at_least(1),
			error
		);
		if (!error || error == boost::asio::error::eof)
		{
			log_->write("[TCPClient doReadSSL] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
				std::to_string(readBuffer_.size())+"] ",
				Log::Level::DEBUG);
		} else
		{
			log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doReadSSL] ") + error.what(), Log::Level::ERROR);
	}
}