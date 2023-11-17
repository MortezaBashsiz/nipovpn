#ifndef PROXY_HPP
#define PROXY_HPP

#include "config.hpp"
#include "log.hpp"
#include "request.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>

class Proxy
{
public:
	Proxy(Config config);
	~Proxy();
	Config nipoConfig;
	Log nipoLog;
	std::string send(request request_);
	std::string sendClientHello(std::string data, std::string server, std::string port);
};

class message
{
public:
  static constexpr std::size_t headerLength = 4;
  static constexpr std::size_t maxBodyLength = 512;

  message()
    : bodyLength_(0)
  {
  }

  const char* data() const
  {
    return data_;
  }

  char* data()
  {
    return data_;
  }

  std::size_t length() const
  {
    return headerLength + bodyLength_;
  }

  const char* body() const
  {
    return data_ + headerLength;
  }

  char* body()
  {
    return data_ + headerLength;
  }

  std::size_t bodyLength() const
  {
    return bodyLength_;
  }

  void bodyLength(std::size_t newLength)
  {
    bodyLength_ = newLength;
    if (bodyLength_ > maxBodyLength)
      bodyLength_ = maxBodyLength;
  }

  bool decodeHeader()
  {
    char header[headerLength + 1] = "";
    std::strncat(header, data_, headerLength);
    bodyLength_ = std::atoi(header);
    if (bodyLength_ > maxBodyLength)
    {
      bodyLength_ = 0;
      return false;
    }
    return true;
  }

  void encodeHeader()
  {
    char header[headerLength + 1] = "";
    std::sprintf(header, "%4d", static_cast<int>(bodyLength_));
    std::memcpy(data_, header, headerLength);
  }

private:
  char data_[headerLength + maxBodyLength];
  std::size_t bodyLength_;
};


typedef std::deque<message> messageQueue;


class SendTcp
{
public:
  SendTcp(boost::asio::io_context& io_context,
      const boost::asio::ip::tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context)
  {
    doConnect(endpoints);
  }

  void write(const message& msg)
  {
    boost::asio::post(io_context_,
        [this, msg]()
        {
          bool write_in_progress = !writeMsgs_.empty();
          writeMsgs_.push_back(msg);
          if (!write_in_progress)
          {
            doWrite();
          }
        });
  }

  void close()
  {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

private:
  void doConnect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
  {
    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::endpoint)
        {
          if (!ec)
          {
            doReadHeader();
          }
        });
  }

  void doReadHeader()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(readMsg_.data(), message::headerLength),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && readMsg_.decodeHeader())
          {
            doReadBody();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void doReadBody()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(readMsg_.body(), readMsg_.bodyLength()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            std::cout.write(readMsg_.body(), readMsg_.bodyLength());
            std::cout << "\n";
            doReadHeader();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void doWrite()
  {
    boost::asio::async_write(socket_,
        boost::asio::buffer(writeMsgs_.front().data(),
          writeMsgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            writeMsgs_.pop_front();
            if (!writeMsgs_.empty())
            {
              doWrite();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

private:
  boost::asio::io_context& io_context_;
  boost::asio::ip::tcp::socket socket_;
  message readMsg_;
  messageQueue writeMsgs_;
};

#endif //PROXY_HPP