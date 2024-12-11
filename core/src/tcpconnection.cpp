#include "tcpconnection.hpp"

/**
 * @brief Constructs a new TCPConnection instance.
 * 
 * Initializes the TCP socket, configuration, log, and client. Sets the UUID for
 * the connection and initializes other state variables like `end_` and `connect_`.
 * 
 * @param io_context The IO context for asynchronous operations.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 * @param client Shared pointer to the associated TCPClient.
 */
TCPConnection::TCPConnection(boost::asio::io_context &io_context,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             TCPClient::pointer client)
    : socket_(io_context),
      config_(config),
      log_(log),
      io_context_(io_context),
      client_(client),
      timeout_(io_context),
      strand_(boost::asio::make_strand(io_context_)) {
    uuid_ = boost::uuids::random_generator()();
    end_ = false;
    connect_ = false;
}

/**
 * @brief Returns the TCP socket associated with this connection.
 * 
 * @return A reference to the TCP socket.
 */
boost::asio::ip::tcp::socket &TCPConnection::socket() {
    return socket_;
}

/**
 * @brief Starts the connection by initiating a read operation.
 * 
 * The connection UUID is assigned to the associated client, and the first read operation is initiated.
 */
void TCPConnection::startAgent() {
    std::lock_guard<std::mutex> lock(mutex_);
    client_->uuid_ = uuid_;
    doReadAgent();
}

/**
 * @brief Starts the connection by initiating a read operation.
 * 
 * The connection UUID is assigned to the associated client, and the first read operation is initiated.
 */
void TCPConnection::startServer() {
    std::lock_guard<std::mutex> lock(mutex_);
    client_->uuid_ = uuid_;
    doReadServer();
}

/**
 * @brief Initiates the read operation from the socket.
 * 
 * The method prepares the buffer and sets up asynchronous read, handling the read operation once completed.
 */
void TCPConnection::doReadAgent() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();
        boost::asio::async_read(
                socket_, readBuffer_, boost::asio::transfer_exactly(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadAgent, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::
                                            bytes_transferred)));
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doReadAgent] [catch] ") + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

/**
 * @brief Initiates the read operation from the socket.
 * 
 * The method prepares the buffer and sets up asynchronous read, handling the read operation once completed.
 */
void TCPConnection::doReadServer() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();
        boost::asio::async_read_until(
                socket_, readBuffer_, "COMP\r\n\r\n",
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadServer, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::
                                            bytes_transferred)));
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doReadServer] [catch] ") + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

/**
 * @brief Handles the completion of the read operation.
 * 
 * If there is an error or EOF, it logs the issue and closes the connection. Otherwise, it reads more data if available
 * and processes it accordingly based on the run mode (agent or server).
 * 
 * @param error The error code indicating the result of the read operation.
 * @param bytes_transferred The number of bytes read.
 */
void TCPConnection::handleReadAgent(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();
        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write(
                        "[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
                socketShutdown();
                return;
            } else {
                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] ") + error.message(),
                        Log::Level::TRACE);
            }
        }

        boost::asio::deadline_timer timer{io_context_};
        resetTimeout();
        std::string bufStr{hexStreambufToStr(readBuffer_)};
        boost::system::error_code errorIn;

        if (bufStr == "43") {
            boost::asio::read_until(socket_, readBuffer_, "\r\n",
                                    errorIn);
        } else {
            boost::asio::streambuf tempTempBuff;
            boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(2),
                              errorIn);
            boost::asio::read(socket_, tempTempBuff, boost::asio::transfer_exactly(2),
                              errorIn);
            unsigned int readSize{hexToInt(hexStreambufToStr(tempTempBuff))};
            moveStreambuf(tempTempBuff, readBuffer_);
            boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(readSize),
                              errorIn);
            timer.expires_from_now(
                    boost::posix_time::milliseconds(config_->general().timeWait));
            timer.wait();
            if (socket_.available() > 0) {
                for (auto i = 0; i <= config_->general().repeatWait; i++) {
                    if (socket_.available() == 0)
                        break;
                    boost::asio::streambuf tempTempBuff;
                    boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(3),
                                      errorIn);
                    boost::asio::read(socket_, tempTempBuff, boost::asio::transfer_exactly(2),
                                      errorIn);
                    unsigned int readSize{hexToInt(hexStreambufToStr(tempTempBuff))};
                    moveStreambuf(tempTempBuff, readBuffer_);
                    boost::asio::read(socket_, readBuffer_, boost::asio::transfer_exactly(readSize),
                                      errorIn);
                }
            }
        }
        OUT(hexStreambufToStr(readBuffer_));

        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);
        log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] " +
                            "[Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::TRACE);

        AgentHandler::pointer agentHandler_ = AgentHandler::create(
                readBuffer_, writeBuffer_, config_, log_, client_,
                socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()),
                uuid_);
        agentHandler_->handle();
        end_ = agentHandler_->end_;
        connect_ = agentHandler_->connect_;
        if (writeBuffer_.size() > 0) {
            doWrite(agentHandler_);
        } else {
            socketShutdown();
        }
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [catch read] ") + error.what(),
                Log::Level::DEBUG);
        socketShutdown();
    }
}

/**
 * @brief Handles the completion of the read operation.
 * 
 * If there is an error or EOF, it logs the issue and closes the connection. Otherwise, it reads more data if available
 * and processes it accordingly based on the run mode (agent or server).
 * 
 * @param error The error code indicating the result of the read operation.
 * @param bytes_transferred The number of bytes read.
 */
void TCPConnection::handleReadServer(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();
        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write(
                        "[" + to_string(uuid_) + "] [TCPConnection handleReadServer] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
                socketShutdown();
                return;
            } else {
                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPConnection handleReadServer] ") + error.message(),
                        Log::Level::TRACE);
            }
        }

        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadServer] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);
        log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] " +
                            "[Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::TRACE);


        ServerHandler::pointer serverHandler_ = ServerHandler::create(
                readBuffer_, writeBuffer_, config_, log_, client_,
                socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()),
                uuid_);
        serverHandler_->handle();
        end_ = serverHandler_->end_;
        connect_ = serverHandler_->connect_;
        if (writeBuffer_.size() > 0) {
            doWrite(serverHandler_);
        } else {
            socketShutdown();
        }

    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection handleReadServer] [catch read] ") + error.what(),
                Log::Level::DEBUG);
        socketShutdown();
    }
}

/**
 * @brief Reads the remaining data from the socket after the initial read.
 * 
 * This method ensures that any remaining data is read from the socket, and it handles possible errors.
 */
void TCPConnection::doReadRest() {
    try {
        readBuffer_.consume(readBuffer_.size());
        boost::system::error_code error;

        resetTimeout();
        boost::asio::read_until(socket_, readBuffer_, "COMP\r\n\r\n",
                                error);

        cancelTimeout();

        try {

            log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [SRC " +
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
                    std::string("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [catch log] ") + error.what(),
                    Log::Level::DEBUG);
        }
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [catch] ") + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

/**
 * @brief Initiates the write operation to the socket.
 * 
 * If the write buffer has data, this method asynchronously writes the data to the socket.
 * 
 * @param handler The handler that will process the write operation.
 */
void TCPConnection::doWrite(auto handlerPointer) {
    boost::system::error_code error;
    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) + "] [TCPConnection doWrite] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }
        resetTimeout();
        if (writeBuffer_.size() > 0) {
            log_->write("[" + to_string(uuid_) + "] [TCPConnection doWrite] [DST " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            log_->write("[" + to_string(uuid_) + "] [Write To] [DST " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                        Log::Level::TRACE);
            boost::asio::write(socket_, writeBuffer_, error);
            cancelTimeout();
            if (!error) {
                if (end_ || connect_) {
                    if (config_->runMode() == RunMode::server)
                        doReadServer();
                    else
                        doReadAgent();
                } else {
                    if (config_->runMode() == RunMode::server)
                        doReadRest();
                    handlerPointer->continueRead();
                    end_ = handlerPointer->end_;
                    doWrite(handlerPointer);
                }
            } else {
                log_->write("[" + to_string(uuid_) + "] [TCPConnection doWrite] [error] " + error.message(),
                            Log::Level::ERROR);
                socketShutdown();
            }
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection doWrite] [catch] ") + error.what(),
                Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::resetTimeout() {
    if (!config_->general().timeout)
        return;
    timeout_.expires_from_now(boost::posix_time::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPConnection::onTimeout,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
}

void TCPConnection::cancelTimeout() {
    if (config_->general().timeout)
        timeout_.cancel();
}

void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error /* No Error */ || error == boost::asio::error::operation_aborted) return;
    log_->write(
            std::string("[" + to_string(uuid_) + "] [TCPConnection onTimeout] [expiration] ") +
                    std::to_string(+config_->general().timeout) +
                    " seconds has passed, and the timeout has expired",
            Log::Level::TRACE);
    socketShutdown();
}

void TCPConnection::socketShutdown() {
    try {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
        socket_.close();
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection socketShutdown] [catch] ") + error.what(),
                Log::Level::DEBUG);
    }
}