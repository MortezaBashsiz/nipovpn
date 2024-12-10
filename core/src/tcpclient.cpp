#include "tcpclient.hpp"

/**
 * @brief Constructs a TCPClient instance with the provided IO context, configuration, and logging.
 * 
 * @param io_context The IO context for asynchronous operations.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 */
TCPClient::TCPClient(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(io_context),
      socket_(io_context),
      resolver_(io_context),
      timeout_(io_context) {
    end_ = false;
}

/**
 * @brief Returns the TCP socket object associated with the TCPClient.
 * 
 * @return A reference to the TCP socket.
 */
boost::asio::ip::tcp::socket &TCPClient::socket() {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_;
}

/**
 * @brief Writes the provided buffer to the TCP socket.
 * 
 * @param buffer The buffer containing data to write.
 */
void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

/**
 * @brief Attempts to establish a TCP connection to the specified destination IP and port.
 * 
 * @param dstIP The destination IP address.
 * @param dstPort The destination port.
 * @return true if the connection is successfully established, false otherwise.
 */
bool TCPClient::doConnect(const std::string &dstIP,
                          const unsigned short &dstPort) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        log_->write("[" + to_string(uuid_) + "] [TCPClient doConnect] [DST " + dstIP + ":" +
                            std::to_string(dstPort) + "]",
                    Log::Level::DEBUG);

        boost::system::error_code error_code;
        auto endpoint = resolver_.resolve(dstIP.c_str(), std::to_string(dstPort).c_str(), error_code);
        if (error_code) {
            return false;
        } else {
            boost::asio::connect(socket_, endpoint);
            return true;
        }
    } catch (std::exception &error) {

        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doConnect] ") + error.what(),
                    Log::Level::ERROR);
        return false;
    }
}


/**
 * @brief Writes the provided buffer to the TCP socket and handles any errors during the process.
 * 
 * @param buffer The buffer containing data to write.
 */
void TCPClient::doWrite(boost::asio::streambuf &buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        moveStreambuf(buffer, writeBuffer_);

        log_->write("[" + to_string(uuid_) + "] [TCPClient doWrite] [DST " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] " +
                            "[Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                    Log::Level::DEBUG);
        log_->write("[" + to_string(uuid_) + "] [Write to] [DST " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] " +
                            "[Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                    Log::Level::TRACE);

        boost::system::error_code error;
        if (writeBuffer_.size() > 0) {
            resetTimeout();
            boost::asio::write(socket_, writeBuffer_, error);
            cancelTimeout();
            if (error) {
                log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doWrite] [error] ") + error.message(),
                            Log::Level::DEBUG);
                socketShutdown();
                return;
            }
        } else {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doWrite] [ZERO Bytes] [DST " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) + "] " +
                                "[Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {

        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doWrite] [catch] ") + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}


/**
 * @brief Reads data from the agent side and processes the received data, logging errors or EOF.
 */
void TCPClient::doReadAgent() {
    boost::system::error_code error;
    std::lock_guard<std::mutex> lock(mutex_);
    readBuffer_.consume(readBuffer_.size());
    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadAgent] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();
        boost::asio::read_until(socket_, readBuffer_, "COMP\r\n\r\n",
                                error);
        cancelTimeout();
        if (error == boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
            socketShutdown();
            return;
        } else if (error) {
            log_->write(
                    std::string("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [error] ") + error.message(),
                    Log::Level::ERROR);
            socketShutdown();
            return;
        }

        if (readBuffer_.size() > 0) {
            copyStreambuf(readBuffer_, buffer_);
            try {

                log_->write("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                            Log::Level::DEBUG);
                log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] " + "[Bytes " + std::to_string(readBuffer_.size()) +
                                    "] ",
                            Log::Level::TRACE);
            } catch (std::exception &error) {

                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [catch log] ") + error.what(),
                        Log::Level::DEBUG);
            }
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {

        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [catch read] ") + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
        return;
    }
}

/**
 * @brief Reads data from the server side and processes the received data, managing retries and timeouts.
 */
void TCPClient::doReadServer() {
    boost::system::error_code error;
    end_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    readBuffer_.consume(readBuffer_.size());
    boost::asio::steady_timer timer(io_context_);

    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();
        boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(2),
                          error);
        std::string bufStr{hexStreambufToStr(readBuffer_)};
        if (bufStr == "1403" || bufStr == "1503" || bufStr == "1603" || bufStr == "1703") {
            boost::asio::streambuf tempTempBuff;
            boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(1),
                              error);
            boost::asio::read(socket_, tempTempBuff, boost::asio::transfer_exactly(2),
                              error);
            unsigned int readSize{hexToInt(hexStreambufToStr(tempTempBuff))};
            moveStreambuf(tempTempBuff, readBuffer_);
            boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(readSize),
                              error);
        }
        cancelTimeout();

        if (error == boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
            socketShutdown();
            return;
        } else if (error) {
            log_->write(
                    std::string("[" + to_string(uuid_) + "] [TCPClient doReadServer] [error] ") + error.message(),
                    Log::Level::ERROR);
            socketShutdown();
            return;
        }

        if (socket_.available() == 0) {
            end_ = true;
        }

        copyStreambuf(readBuffer_, buffer_);
        try {

            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [SRC " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] " + "[Bytes " + std::to_string(readBuffer_.size()) +
                                "] ",
                        Log::Level::TRACE);
        } catch (std::exception &error) {

            log_->write(
                    std::string("[" + to_string(uuid_) + "] [TCPClient doReadServer] [catch log] ") + error.what(),
                    Log::Level::DEBUG);
        }

    } catch (std::exception &error) {

        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doReadServer] [catch read] ") + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
        return;
    }
}

void TCPClient::resetTimeout() {
    if (!config_->general().timeout)
        return;
    timeout_.expires_from_now(boost::posix_time::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPClient::onTimeout,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
}

void TCPClient::cancelTimeout() {

    if (config_->general().timeout)
        timeout_.cancel();
}

void TCPClient::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;

    log_->write(
            std::string("[" + to_string(uuid_) + "] [TCPClient onTimeout] [expiration] ") +
                    std::to_string(+config_->general().timeout) +
                    " seconds has passed, and the timeout has expired",
            Log::Level::TRACE);
    socketShutdown();
}

void TCPClient::socketShutdown() {
    try {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
        socket_.close();
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPClient socketShutdown] [catch] ") + error.what(),
                Log::Level::DEBUG);
    }
}