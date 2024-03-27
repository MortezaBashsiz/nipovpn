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
		io_context_(io_context),
		client_(client)
{ }

boost::asio::ip::tcp::socket& TCPConnection::socket()
{
	return socket_;
}

void TCPConnection::start()
{
	if (config_->runMode() == RunMode::agent)
		doReadUntil("\r\n\r\n");
	else if (config_->runMode() == RunMode::server)
		doReadUntil("\r\nCOMP\r\n\r\n");
}

void TCPConnection::doReadUntil(const std::string& until)
{
	try
	{
		log_->write("[TCPConnection doReadUntil] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"]",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection doReadUntil] [log] ") + error.what(), Log::Level::ERROR);
	}

	boost::asio::async_read_until(
		socket_,
		readBuffer_,
		until,
		boost::bind(&TCPConnection::handleRead, 
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)
	);
}

void TCPConnection::doRead(const unsigned short& bytes, boost::asio::streambuf& buffer)
{
	try
	{
		buffer.consume(buffer.size());
		boost::system::error_code error;
		boost::asio::read(
			socket_,
			buffer,
			boost::asio::transfer_exactly(bytes),
			error
		);
		if (error && error != boost::asio::error::eof)
		{
			log_->write(std::string("[TCPConnection doRead bytes] ") + error.what(), Log::Level::ERROR);
		}
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPConnection::doReadSSL()
{
	try
	{
		readBuffer_.consume(readBuffer_.size());
		boost::asio::async_read(
		socket_,
		readBuffer_,
		boost::asio::transfer_exactly(1),
		boost::bind(&TCPConnection::handleReadSSL, 
			shared_from_this(),
			boost::asio::placeholders::error)
		);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doReadSSL] ") + error.what(), Log::Level::ERROR);
	}
}

void TCPConnection::handleRead(const boost::system::error_code& error,
	size_t bytes_transferred)
{
	try
	{
		log_->write("[TCPConnection handleRead] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
			std::to_string(bytes_transferred)+"] ",
			Log::Level::INFO);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPConnection handleRead] [log] ") + error.what(), Log::Level::ERROR);
	}
	if (!error || error == boost::asio::error::eof)
	{
		if (config_->runMode() == RunMode::agent)
		{
			AgentHandler::pointer agentHandler_ = AgentHandler::create(readBuffer_, writeBuffer_, config_, log_, client_);
			agentHandler_->handle();
			doWrite();
			if (agentHandler_->request()->httpType() == HTTP::HttpType::https || 
					agentHandler_->request()->httpType() == HTTP::HttpType::connect)
			{
				doReadSSL();
			}
		} else if (config_->runMode() == RunMode::server)
		{
			ServerHandler::pointer serverHandler_ = ServerHandler::create(readBuffer_, writeBuffer_, config_, log_,  client_);
			serverHandler_->handle();
			doWrite();
			if (serverHandler_->request()->httpType() == HTTP::HttpType::https || 
					serverHandler_->request()->httpType() == HTTP::HttpType::connect)
			{
				readBuffer_.consume(readBuffer_.size());
				doReadUntil("\r\nCOMP\r\n\r\n");
			}
		}
	} else
	{
		log_->write(std::string("[TCPConnection handleRead] ") + error.what(), Log::Level::ERROR);
	}
}


void TCPConnection::handleReadSSL(const boost::system::error_code& error)
{
	if (!error || error == boost::asio::error::eof)
	{
		while(true){
			boost::system::error_code error;
			boost::asio::read(
				socket_,
				readBuffer_,
				boost::asio::transfer_exactly(2),
				error
			);
			boost::asio::streambuf tempBuff;
			boost::asio::read(
				socket_,
				tempBuff,
				boost::asio::transfer_exactly(2),
				error
			);
			auto bytesToRead = hexToInt(hexStreambufToStr(tempBuff));
			moveStreamBuff(tempBuff, readBuffer_);
			auto size = boost::asio::read(
				socket_,
				readBuffer_,
				boost::asio::transfer_exactly(bytesToRead),
				error
			);
			if (socket_.available() != 0)
			{
				boost::asio::read(
					socket_,
					readBuffer_,
					boost::asio::transfer_exactly(1),
					error
				);
				continue;
			} 
			else if (error == boost::asio::error::eof || size == 0 || socket_.available() == 0 )
				break;
			else if (error)
			{
				log_->write(std::string("[TCPConnection handleReadSSL] [log] ") + error.what(), Log::Level::ERROR);
			}
		}
		try
		{
			log_->write("[TCPConnection handleReadSSL] [SRC " +
				socket_.remote_endpoint().address().to_string() +":"+
				std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
				std::to_string(readBuffer_.size())+"] ",
				Log::Level::INFO);
		}
		catch (std::exception& error)
		{
			log_->write(std::string("[TCPConnection handleReadSSL] [log] ") + error.what(), Log::Level::ERROR);
		}
		if (config_->runMode() == RunMode::agent)
		{
			AgentHandler::pointer agentHandler_ = AgentHandler::create(readBuffer_, writeBuffer_, config_, log_, client_);
			agentHandler_->handle();
			doWrite();
			if (agentHandler_->request()->httpType() == HTTP::HttpType::https || 
					agentHandler_->request()->httpType() == HTTP::HttpType::connect)
			{
				doReadSSL();
			}
		} else if (config_->runMode() == RunMode::server)
		{
			ServerHandler::pointer serverHandler_ = ServerHandler::create(readBuffer_, writeBuffer_, config_, log_,  client_);
			serverHandler_->handle();
			doWrite();
			if (serverHandler_->request()->httpType() == HTTP::HttpType::https || 
					serverHandler_->request()->httpType() == HTTP::HttpType::connect)
			{
				readBuffer_.consume(readBuffer_.size());
				doReadUntil("\r\nCOMP\r\n\r\n");
			}
		}
	} else
	{
		log_->write(std::string("[TCPConnection handleReadSSL] ") + error.what(), Log::Level::ERROR);
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