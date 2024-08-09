#include "tcpclient.hpp"

/*
 * TCPClient
 */

/*
 * Constructor for the TCPClient class.
 * Initializes the TCP client with given configuration and logging objects,
 * and sets up the required I/O context, socket, resolver, and timer.
 *
 * @param io_context - The Boost Asio I/O context to be used for asynchronous
 * operations.
 * @param config - Shared pointer to the configuration object.
 * @param log - Shared pointer to the logging object.
 */
TCPClient::TCPClient(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(io_context),
      socket_(io_context),
      resolver_(io_context),
      timer_(io_context) {}

/*
 * Getter for the socket.
 *
 * @return Reference to the Boost Asio TCP socket.
 */
boost::asio::ip::tcp::socket &TCPClient::socket() { return socket_; }

/*
 * Moves the contents of the given stream buffer to the internal write buffer.
 *
 * @param buffer - Stream buffer containing data to be written.
 */
void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
  moveStreambuf(buffer, writeBuffer_);
}

/*
 * Connects the TCP client to a specified destination IP and port.
 * Logs the connection attempt and handles any connection errors.
 *
 * @param dstIP - Destination IP address as a string.
 * @param dstPort - Destination port number.
 */
bool TCPClient::doConnect(const std::string &dstIP,
                          const unsigned short &dstPort) {
  try {
    // Log connection attempt
    log_->write("[TCPClient doConnect] [DST " + dstIP + ":" +
                    std::to_string(dstPort) + "]",
                Log::Level::DEBUG);

    // Resolve the endpoint and connect
    boost::asio::ip::tcp::resolver resolver(io_context_);
    boost::system::error_code error_code;
    auto endpoint = resolver.resolve(
        dstIP.c_str(), std::to_string(dstPort).c_str(), error_code);
    if (error_code) {
      return false;
    } else {
      boost::asio::connect(socket_, endpoint);
      return true;
    }
  } catch (std::exception &error) {
    // Log connection errors
    log_->write(std::string("[TCPClient doConnect] ") + error.what(),
                Log::Level::ERROR);
    return false;
  }
}

/*
 * Writes data from the internal write buffer to the connected socket.
 * Logs the write operation and handles any write errors.
 *
 * @param buffer - Stream buffer containing data to be written.
 */
void TCPClient::doWrite(boost::asio::streambuf &buffer) {
  try {
    moveStreambuf(buffer, writeBuffer_);
    // Log details of the write operation
    log_->write("[TCPClient doWrite] [DST " +
                    socket_.remote_endpoint().address().to_string() + ":" +
                    std::to_string(socket_.remote_endpoint().port()) + "] " +
                    "[Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                Log::Level::DEBUG);
    log_->write("[Write to] [DST " +
                    socket_.remote_endpoint().address().to_string() + ":" +
                    std::to_string(socket_.remote_endpoint().port()) + "] " +
                    "[Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                Log::Level::TRACE);
    // Perform the write operation
    boost::system::error_code error;
    boost::asio::write(socket_, writeBuffer_, error);
    if (error) {
      log_->write(std::string("[TCPClient doWrite] [error] ") + error.message(),
                  Log::Level::DEBUG);
      socket_.close();  // Close socket on write error
    }
  } catch (std::exception &error) {
    // Log exceptions during the write operation
    log_->write(std::string("[TCPClient doWrite] [catch] ") + error.what(),
                Log::Level::DEBUG);
    socket_.close();  // Ensure socket closure on exception
  }
}

/*
 * Reads data from the connected socket into the internal read buffer.
 * Handles data reading, potential errors, and logs the read operation.
 * Implements a retry mechanism based on configuration settings.
 */
void TCPClient::doRead() {
  try {
    // Clear the read buffer
    readBuffer_.consume(readBuffer_.size());
    boost::system::error_code error;
    // Read at least 1 byte from the socket
    boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(1),
                      error);

    if (socket_.available() > 0) {
      // Implement retry mechanism if data is still available
      boost::asio::deadline_timer timer{io_context_};
      for (auto i = 0; i <= config_->general().repeatWait; i++) {
        while (true) {
          if (socket_.available() == 0) break;
          boost::asio::read(socket_, readBuffer_,
                            boost::asio::transfer_exactly(1), error);
          if (error == boost::asio::error::eof) {
            log_->write(
                "[TCPClient doRead] [EOF] Connection "
                "closed by peer.",
                Log::Level::TRACE);
            socket_.close();
            return;  // Exit after closing the socket
          } else if (error) {
            log_->write(
                std::string("[TCPClient doRead] [error] ") + error.message(),
                Log::Level::ERROR);
            socket_.close();
            return;  // Exit after closing the socket
          }
        }
        timer.expires_from_now(
            boost::posix_time::milliseconds(config_->general().timeWait));
        timer.wait();
      }
    }

    if (readBuffer_.size() > 0) {
      try {
        // Log the successful read operation
        log_->write("[TCPClient doRead] [SRC " +
                        socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()) +
                        "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);
        log_->write("[Read from] [SRC " +
                        socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()) +
                        "] " + "[Bytes " + std::to_string(readBuffer_.size()) +
                        "] ",
                    Log::Level::TRACE);
      } catch (std::exception &error) {
        // Log exceptions during logging
        log_->write(
            std::string("[TCPClient doRead] [catch log] ") + error.what(),
            Log::Level::DEBUG);
      }
    } else {
      socket_.close();  // Close socket if no data is read
    }
  } catch (std::exception &error) {
    // Log exceptions during the read operation
    log_->write(std::string("[TCPClient doRead] [catch read] ") + error.what(),
                Log::Level::ERROR);
    socket_.close();  // Ensure socket closure on error
    return;
  }
}
