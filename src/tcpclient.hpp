#pragma once
#ifndef TCPCLIENT_HPP
#define TCPCLIENT_HPP

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"

/*
 * TCPClient is a class for managing a TCP client connection.
 * It handles connecting to an endpoint, sending data, and receiving data.
 */
class TCPClient : public boost::enable_shared_from_this<TCPClient>
{
public:
  // Type alias for shared pointer to TCPClient
  using pointer = boost::shared_ptr<TCPClient>;

  /*
   * Factory method to create a new TCPClient instance.
   *
   * @param io_context - The Boost Asio I/O context for asynchronous
   * operations.
   * @param config - Shared pointer to the configuration object.
   * @param log - Shared pointer to the logging object.
   * @return Shared pointer to a new TCPClient instance.
   */
  static pointer
  create (boost::asio::io_context &io_context,
          const std::shared_ptr<Config> &config,
          const std::shared_ptr<Log> &log)
  {
    return pointer (new TCPClient (io_context, config, log));
  }

  /*
   * Getter for the socket.
   *
   * @return Reference to the Boost Asio TCP socket.
   */
  boost::asio::ip::tcp::socket &socket ();

  /*
   * Moves data from a given stream buffer to the internal write buffer.
   *
   * @param buffer - Stream buffer containing data to be written.
   */
  void writeBuffer (boost::asio::streambuf &buffer);

  /*
   * Accessors for the internal buffers.
   *
   * @return Reference or rvalue reference to the internal write buffer.
   */
  inline boost::asio::streambuf &
  writeBuffer () &
  {
    return writeBuffer_;
  }
  inline boost::asio::streambuf &&
  writeBuffer () &&
  {
    return std::move (writeBuffer_);
  }

  /*
   * Accessors for the internal buffers.
   *
   * @return Reference or rvalue reference to the internal read buffer.
   */
  inline boost::asio::streambuf &
  readBuffer () &
  {
    return readBuffer_;
  }
  inline boost::asio::streambuf &&
  readBuffer () &&
  {
    return std::move (readBuffer_);
  }

  /*
   * Initiates a connection to the specified destination IP and port.
   *
   * @param dstIP - Destination IP address as a string.
   * @param dstPort - Destination port number.
   */
  void doConnect (const std::string &dstIP, const unsigned short &dstPort);

  /*
   * Writes data from the internal write buffer to the connected socket.
   *
   * @param buffer - Stream buffer containing data to be written.
   */
  void doWrite (boost::asio::streambuf &buffer);

  /*
   * Reads data from the connected socket into the internal read buffer.
   */
  void doRead ();

private:
  /*
   * Private constructor for TCPClient.
   * Initializes the TCP client with the given I/O context, configuration, and
   * logging objects.
   *
   * @param io_context - The Boost Asio I/O context for asynchronous
   * operations.
   * @param config - Shared pointer to the configuration object.
   * @param log - Shared pointer to the logging object.
   */
  explicit TCPClient (boost::asio::io_context &io_context,
                      const std::shared_ptr<Config> &config,
                      const std::shared_ptr<Log> &log);

  // Members
  const std::shared_ptr<Config>
      &config_;                     // Reference to the configuration object
  const std::shared_ptr<Log> &log_; // Reference to the logging object
  boost::asio::io_context &io_context_;     // Reference to the I/O context
  boost::asio::ip::tcp::socket socket_;     // TCP socket for communication
  boost::asio::streambuf writeBuffer_;      // Buffer for data to be written
  boost::asio::streambuf readBuffer_;       // Buffer for data to be read
  boost::asio::ip::tcp::resolver resolver_; // Resolver for endpoint resolution
  boost::asio::deadline_timer timer_;       // Timer for managing timeouts
};

#endif /* TCPCLIENT_HPP */
