#include "tcpconnection.hpp"

/*
 * TCPConnection
 */
TCPConnection::TCPConnection(boost::asio::io_context &io_context,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             const TCPClient::pointer &client)
    : socket_(io_context),    // Initialize the TCP socket with the I/O context
      config_(config),        // Store the configuration
      log_(log),              // Store the logging object
      io_context_(io_context),// Store the I/O context
      client_(client),        // Store the TCP client pointer
      timer_(io_context),     // Initialize the deadline timer with the I/O context
      strand_(boost::asio::make_strand(io_context_)) {
    uuid_ = boost::uuids::random_generator()();
}

boost::asio::ip::tcp::socket &TCPConnection::socket() {
    return socket_;// Return the TCP socket
}

void TCPConnection::start() {
    std::lock_guard<std::mutex> lock(mutex_);// Lock the mutex for thread safety

    client_->uuid_ = uuid_;
    doRead();// Start reading from the socket
}

void TCPConnection::doRead() {
    try {
        readBuffer_.consume(readBuffer_.size());  // Clear the read buffer
        writeBuffer_.consume(writeBuffer_.size());// Clear the write buffer
        boost::asio::async_read(
                socket_, readBuffer_, boost::asio::transfer_exactly(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleRead, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::
                                            bytes_transferred)));// Start an asynchronous
                                                                 // read operation
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doRead] [catch] ") + error.what(),
                    Log::Level::ERROR);// Log any exceptions
        socket_.close();
    }
}

void TCPConnection::handleRead(const boost::system::error_code &error, size_t) {
    try {
        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write(
                        "[" + to_string(uuid_) + "] [TCPConnection handleRead] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
                socket_.close();// Close the socket on error or EOF
                return;
            } else {
                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPConnection handleRead] ") + error.message(),
                        Log::Level::TRACE);
            }
        }

        if (socket_.available() > 0) {
            boost::asio::deadline_timer timer{io_context_};
            for (auto i = 0; i <= config_->general().repeatWait; i++) {
                while (true) {
                    if (socket_.available() == 0)
                        break;// Exit if no more data is available
                    boost::system::error_code read_error;
                    boost::asio::read(socket_, readBuffer_,
                                      boost::asio::transfer_exactly(1), read_error);
                    if (read_error == boost::asio::error::eof) {
                        log_->write(
                                "[" + to_string(uuid_) + "] [TCPConnection handleRead] [EOF] Connection closed by peer.",
                                Log::Level::DEBUG);
                        socket_.close();// Close the socket on EOF
                        return;
                    } else if (read_error) {
                        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection handleRead] [error] ") +
                                            read_error.message(),
                                    Log::Level::ERROR);
                        socket_.close();// Close the socket on any other errors
                        return;
                    }
                }
                timer.expires_from_now(
                        boost::posix_time::milliseconds(config_->general().timeWait));
                timer.wait();// Wait for the specified timeout duration
            }

            // Log the successful read operation
            log_->write("[" + to_string(uuid_) + "] [TCPConnection handleRead] [SRC " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) + "] " +
                                "[Bytes " + std::to_string(readBuffer_.size()) + "] ",
                        Log::Level::TRACE);
            // Handle the request based on the running mode
            if (config_->runMode() == RunMode::agent) {
                AgentHandler::pointer agentHandler_ = AgentHandler::create(
                        readBuffer_, writeBuffer_, config_, log_, client_,
                        socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()),
                        uuid_);
                agentHandler_->handle();// Process the request as an agent
            } else if (config_->runMode() == RunMode::server) {
                ServerHandler::pointer serverHandler_ = ServerHandler::create(
                        readBuffer_, writeBuffer_, config_, log_, client_,
                        socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()),
                        uuid_);
                serverHandler_->handle();// Process the request as a server
            }

            if (writeBuffer_.size() > 0) {
                doWrite();// Write to the socket if there is data in the write
                          // buffer
            } else {
                socket_.close();// Close the socket if no data to write
            }
        }
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection handleRead] [catch read] ") + error.what(),
                Log::Level::DEBUG);
        socket_.close();// Ensure socket closure on exception
    }
}

void TCPConnection::doWrite() {
    std::lock_guard<std::mutex> lock(mutex_);// Lock the mutex for thread safety
    try {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doWrite] [DST " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                    Log::Level::DEBUG);
        log_->write("[" + to_string(uuid_) + "] [Write to] [DST " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                    Log::Level::TRACE);// Log the details of the write operation
        boost::system::error_code error;
        boost::asio::write(socket_, writeBuffer_,
                           error);// Write data to the socket
        if (error) {
            log_->write(
                    std::string("[" + to_string(uuid_) + "] [TCPConnection doWrite] [error] ") + error.message(),
                    Log::Level::ERROR);// Log any errors during the write operation
            socket_.close();           // Close the socket on error
            return;
        }
        doRead();// Continue reading after writing
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection doWrite] [catch] ") + error.what(),
                Log::Level::ERROR);// Log any exceptions during the write operation
        socket_.close();           // Ensure socket closure on exception
    }
}
