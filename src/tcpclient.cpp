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
	copyStreamBuff(buffer, writeBuffer_);
}

boost::asio::streambuf& TCPClient::readBuffer()
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
	boost::asio::connect(socket_, endpoint);
}

void TCPClient::doConnect(const std::string& dstIP, const unsigned short& dstPort)
{
	log_->write("[TCPClient doConnect] [DST] " +
		dstIP +":"+
		std::to_string(dstPort)+" ",
		Log::Level::DEBUG);

	boost::asio::ip::tcp::resolver resolver(io_context_);
	auto endpoint = resolver.resolve(dstIP.c_str(), std::to_string(dstPort).c_str());
	boost::asio::connect(socket_, endpoint);
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
		log_->write(std::string(" [TCPClient doWrite] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPClient::doRead()
{
	try
	{
		boost::asio::read(
			socket_,
			readBuffer_
		);
		log_->write("[TCPClient doRead] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(readBuffer_.size())+"] ",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string(" [TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}