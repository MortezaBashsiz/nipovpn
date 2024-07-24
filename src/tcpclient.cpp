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
    resolver_(io_context),
    timer_(io_context)
{ }

boost::asio::ip::tcp::socket& TCPClient::socket()
{
  return socket_;
}

void TCPClient::writeBuffer(boost::asio::streambuf& buffer)
{
  moveStreamBuff(buffer, writeBuffer_);
}

void TCPClient::doConnect(const std::string& dstIP, const unsigned short& dstPort)
{
  try
  {
    log_->write("[TCPClient doConnect] [DST " +
      dstIP +":"+
      std::to_string(dstPort)+" ]",
      Log::Level::DEBUG);

    log_->write("[Connect to] [DST " +
      dstIP +":"+
      std::to_string(dstPort)+" ]",
      Log::Level::INFO);
  
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoint = resolver.resolve(dstIP.c_str(), std::to_string(dstPort).c_str());
    boost::asio::connect(socket_, endpoint);
    boost::asio::socket_base::keep_alive option(true);
    socket_.set_option(option);
  }
  catch (std::exception& error)
  {
    log_->write(std::string("[TCPClient doConnect] ") + error.what(), Log::Level::ERROR);
  }
}

void TCPClient::doWrite(boost::asio::streambuf& buffer)
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
    log_->write("[Write to] [DST " +
      socket_.remote_endpoint().address().to_string() +":"+
      std::to_string(socket_.remote_endpoint().port())+"] "+
      "[Bytes " +
      std::to_string(writeBuffer_.size())+"] ",
      Log::Level::TRACE);
    boost::system::error_code error;
    boost::asio::write(
      socket_,
      writeBuffer_,
      error
    );
    if (error){
      log_->write(std::string("[TCPClient doWrite] [error] ") + error.what(), Log::Level::ERROR);
    }
  }
  catch (std::exception& error)
  {
    log_->write(std::string("[TCPClient doWrite] [catch] ") + error.what(), Log::Level::ERROR);
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
        boost::asio::transfer_exactly(1),
        error
      );
    if (socket_.available() > 0)
    {
      for (auto i=0; i<=config_->general().repeatWait; i++)
      {
        while(true)
        {
          if (socket_.available() == 0)
            break;
          boost::asio::read(
            socket_,
            readBuffer_,
            boost::asio::transfer_exactly(1),
            error
          );
          if (error == boost::asio::error::eof)
            break;
          else if (error)
          {
            log_->write(std::string("[TCPClient doRead] [error] ") + error.what(), Log::Level::ERROR);
          }
        }
        timer_.expires_from_now(boost::posix_time::milliseconds(config_->general().timeWait));
        timer_.wait();
      }
    }
    if (readBuffer_.size() > 0)
    {
      try
      {
        log_->write("[TCPClient doRead] [SRC " +
          socket_.remote_endpoint().address().to_string() +":"+
          std::to_string(socket_.remote_endpoint().port())+"] [Bytes "+
          std::to_string(readBuffer_.size())+"] ",
          Log::Level::DEBUG);
      }
      catch (std::exception& error)
      {
        log_->write(std::string("[TCPClient doRead] [catch] ") + error.what(), Log::Level::ERROR);
      }
    } else
      socket_.close();
  }
  catch (std::exception& error)
  {
    log_->write(std::string("[TCPClient doRead] [catch] ") + error.what(), Log::Level::ERROR);
  }
}