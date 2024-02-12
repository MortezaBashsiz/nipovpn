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
		switch (httpType){
			case HTTP::HttpType::https:
				if (config_->runMode() == RunMode::agent)
					doReadUntil("\r\nCOMP\r\n\r\n");
				else
					doReadSSL();
			break;
			case HTTP::HttpType::http:
				doRead();
			break;
			case HTTP::HttpType::connect:
				doReadUntil("\r\n\r\n");
			break;
		}
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

void TCPClient::doRead(const unsigned short& bytes, boost::asio::streambuf& buffer)
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
			log_->write(std::string("[TCPClient doRead bytes] ") + error.what(), Log::Level::ERROR);
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
		boost::asio::streambuf tempBuff;
		unsigned short readExactly{1};
		bool end{false};
		bool isApplicationData{false};
		bool isApplicationDataVersion{false};
		bool isApplicationDataSize{false};

		while(!end){
			doRead(readExactly, tempBuff);
			if (!error)
			{
				std::string tempBuffStr{hexStreambufToStr(tempBuff)};
				if (tempBuffStr == "16")
				{
					copyStreamBuff(tempBuff, readBuffer_);
					boost::asio::streambuf internalTempBuff;
					doRead(2, internalTempBuff);
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(2, internalTempBuff);
					std::string internalTempBuffStr{hexStreambufToStr(internalTempBuff)};
					unsigned short newReadExactly{hexToInt(internalTempBuffStr)};
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(newReadExactly, internalTempBuff);
					std::string finalTempBuffStr = hexArrToStr(
						reinterpret_cast<unsigned char*>(
							const_cast<char*>(
								streambufToString(internalTempBuff).c_str()
							)
						),
						internalTempBuff.size()
					);
					if (finalTempBuffStr == "0e000000")
					{
						end = true;
					}
					copyStreamBuff(internalTempBuff, readBuffer_);
					continue;
				}
				if (tempBuffStr == "14")
				{
					copyStreamBuff(tempBuff, readBuffer_);
					boost::asio::streambuf internalTempBuff;
					doRead(2, internalTempBuff);
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(2, internalTempBuff);
					std::string internalTempBuffStr{hexStreambufToStr(internalTempBuff)};
					unsigned short newReadExactly{hexToInt(internalTempBuffStr)};
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(newReadExactly, internalTempBuff);
					copyStreamBuff(internalTempBuff, readBuffer_);
					continue;
				}
				if (tempBuffStr == "17" && !isApplicationData)
				{
					isApplicationData = true;
					copyStreamBuff(tempBuff, readBuffer_);
					boost::asio::streambuf internalTempBuff;
					doRead(2, internalTempBuff);
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(2, internalTempBuff);
					std::string internalTempBuffStr{hexStreambufToStr(internalTempBuff)};
					unsigned short newReadExactly{hexToInt(internalTempBuffStr)};
					copyStreamBuff(internalTempBuff, readBuffer_);
					doRead(newReadExactly, internalTempBuff);
					copyStreamBuff(internalTempBuff, readBuffer_);
					end = true;
					continue;
				}
			} else if (error == boost::asio::error::eof)
			{
				end = true;
			} else
			{
				end = true;
				log_->write(std::string("[TCPClient doRead] ") + error.what(), Log::Level::ERROR);
			}
		}
		log_->write("[TCPClient doReadSSL] [SRC " +
			socket_.remote_endpoint().address().to_string() +":"+
			std::to_string(socket_.remote_endpoint().port())+"] [Bytes " +
			std::to_string(readBuffer_.size())+"] ",
			Log::Level::DEBUG);
	}
	catch (std::exception& error)
	{
		log_->write(std::string("[TCPClient doReadSSL] ") + error.what(), Log::Level::ERROR);
	}
}