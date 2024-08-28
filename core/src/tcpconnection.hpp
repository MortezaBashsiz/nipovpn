#pragma once
#ifndef TCPCONNECTION_HPP
#define TCPCONNECTION_HPP

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

/*
 * This class handles a single TCP connection.
 * It is responsible for managing the TCP socket and its associated
 * buffers, and it processes incoming and outgoing data.
 */
class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
 public:
  using pointer =
      boost::shared_ptr<TCPConnection>;  // Define a type alias for a shared
                                         // pointer to TCPConnection

  // Factory method to create a new TCPConnection instance
  static pointer create(boost::asio::io_context &io_context,
                        const std::shared_ptr<Config> &config,
                        const std::shared_ptr<Log> &log,
                        const TCPClient::pointer &client) {
    return pointer(new TCPConnection(io_context, config, log, client));
  }

  // Provides access to the TCP socket
  boost::asio::ip::tcp::socket &socket();

  // Moves the content of the given buffer to the write buffer
  inline void writeBuffer(boost::asio::streambuf &buffer) {
    moveStreambuf(buffer, writeBuffer_);
  }

  // Accessors for the read buffer
  inline const boost::asio::streambuf &readBuffer() & { return readBuffer_; }
  inline const boost::asio::streambuf &&readBuffer() && {
    return std::move(readBuffer_);
  }

  // Starts the reading process
  void start();

  // Initiates an asynchronous read operation
  void doRead();

  // Handles the completion of an asynchronous read operation
  void handleRead(const boost::system::error_code &error,
                  size_t bytes_transferred);

  // Initiates an asynchronous write operation
  void doWrite();

 private:
  // Private constructor to enforce use of the factory method
  explicit TCPConnection(boost::asio::io_context &io_context,
                         const std::shared_ptr<Config> &config,
                         const std::shared_ptr<Log> &log,
                         const TCPClient::pointer &client);

  boost::asio::ip::tcp::socket socket_;    // The TCP socket for this connection
  const std::shared_ptr<Config> &config_;  // Configuration settings
  const std::shared_ptr<Log> &log_;        // Logging object
  boost::asio::io_context &io_context_;    // The I/O context
  const TCPClient::pointer &client_;  // Pointer to the associated TCP client
  boost::asio::streambuf readBuffer_,
      writeBuffer_;                    // Buffers for reading and writing data
  boost::asio::deadline_timer timer_;  // Timer used for managing timeouts

  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};

#endif /* TCPCONNECTION_HPP */
