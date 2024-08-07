#include "tcpconnection.hpp"

/*
 * TCPConnection
 */
TCPConnection::TCPConnection(boost::asio::io_context& io_context,
                             const std::shared_ptr<Config>& config,
                             const std::shared_ptr<Log>& log,
                             const TCPClient::pointer& client)
    : socket_(io_context),  // Initialize the TCP socket with the I/O context
      config_(config),      // Store the configuration
      log_(log),            // Store the logging object
      io_context_(io_context),  // Store the I/O context
      client_(client),          // Store the TCP client pointer
      timer_(io_context)  // Initialize the deadline timer with the I/O context
{}

boost::asio::ip::tcp::socket& TCPConnection::socket() {
  return socket_;  // Return the TCP socket
}

void TCPConnection::start() {
  doRead();  // Start reading from the socket
}

void TCPConnection::doRead() {
  try {
    readBuffer_.consume(readBuffer_.size());  // Clear the read buffer
    boost::asio::async_read(
        socket_, readBuffer_, boost::asio::transfer_exactly(1),
        boost::bind(
            &TCPConnection::handleRead, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::
                bytes_transferred));  // Start an asynchronous read operation
  } catch (std::exception& error) {
    log_->write(std::string("[TCPConnection doRead] [catch] ") + error.what(),
                Log::Level::ERROR);  // Log any exceptions
  }
}

void TCPConnection::handleRead(const boost::system::error_code&, size_t) {
  try {
    boost::system::error_code error;
    if (socket_.available() > 0) {  // Check if there is data available to read
      boost::asio::deadline_timer timer{io_context_};
      for (auto i = 0; i <= config_->general().repeatWait; i++) {
        while (true) {
          if (socket_.available() == 0)
            break;  // Exit if no more data is available
          boost::asio::read(socket_, readBuffer_,
                            boost::asio::transfer_exactly(1), error);
          if (error == boost::asio::error::eof) {
            socket_.close();  // Close the socket on EOF
            break;
          } else if (error) {
            socket_.close();  // Close the socket on any other errors
            log_->write(std::string("[TCPConnection handleRead] [error] ") +
                            error.what(),
                        Log::Level::ERROR);
          }
        }
        timer.expires_from_now(
            boost::posix_time::milliseconds(config_->general().timeWait));
        timer.wait();  // Wait for the specified timeout duration
      }
    }
    try {
      log_->write("[TCPConnection handleRead] [SRC " +
                      socket_.remote_endpoint().address().to_string() + ":" +
                      std::to_string(socket_.remote_endpoint().port()) +
                      "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                  Log::Level::DEBUG);
      log_->write("[Read from] [SRC " +
                      socket_.remote_endpoint().address().to_string() + ":" +
                      std::to_string(socket_.remote_endpoint().port()) +
                      "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                  Log::Level::TRACE);  // Log the details of the read operation
    } catch (std::exception& error) {
      log_->write(
          std::string("[TCPConnection handleRead] [catch log] ") + error.what(),
          Log::Level::DEBUG);
    }
    // Handle the request based on the running mode
    if (config_->runMode() == RunMode::agent) {
      AgentHandler::pointer agentHandler_ = AgentHandler::create(
          readBuffer_, writeBuffer_, config_, log_, client_,
          socket_.remote_endpoint().address().to_string() + ":" +
              std::to_string(socket_.remote_endpoint().port()));
      agentHandler_->handle();  // Process the request as an agent
    } else if (config_->runMode() == RunMode::server) {
      ServerHandler::pointer serverHandler_ = ServerHandler::create(
          readBuffer_, writeBuffer_, config_, log_, client_);
      serverHandler_->handle();  // Process the request as a server
    }
    if (writeBuffer_.size() > 0)
      doWrite();  // Write to the socket if there is data in the write buffer
    else {
      socket_.close();  // Close the socket if no data to write
    }
  } catch (std::exception& error) {
    log_->write(
        std::string("[TCPConnection handleRead] [catch read] ") + error.what(),
        Log::Level::ERROR);  // Log any exceptions during the read handling
  }
}

void TCPConnection::doWrite() {
  try {
    log_->write("[TCPConnection doWrite] [DST " +
                    socket_.remote_endpoint().address().to_string() + ":" +
                    std::to_string(socket_.remote_endpoint().port()) +
                    "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                Log::Level::DEBUG);
    log_->write("[Write to] [DST " +
                    socket_.remote_endpoint().address().to_string() + ":" +
                    std::to_string(socket_.remote_endpoint().port()) +
                    "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                Log::Level::TRACE);  // Log the details of the write operation
    boost::system::error_code error;
    boost::asio::write(socket_, writeBuffer_,
                       error);  // Write data to the socket
    if (error) {
      log_->write(
          std::string("[TCPConnection doWrite] [error] ") + error.what(),
          Log::Level::ERROR);  // Log any errors during the write operation
    }
    doRead();  // Continue reading after writing
  } catch (std::exception& error) {
    log_->write(
        std::string("[TCPConnection doWrite] [catch] ") + error.what(),
        Log::Level::ERROR);  // Log any exceptions during the write operation
  }
}
