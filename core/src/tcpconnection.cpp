#include "tcpconnection.hpp"

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

boost::asio::ip::tcp::socket &TCPConnection::socket() {
    return socket_;
}

void TCPConnection::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    client_->uuid_ = uuid_;
    doRead();
}

void TCPConnection::doRead() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();
        boost::asio::async_read(
                socket_, readBuffer_, boost::asio::transfer_exactly(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleRead, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::
                                            bytes_transferred)));
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doRead] [catch] ") + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleRead(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();
        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write(
                        "[" + to_string(uuid_) + "] [TCPConnection handleRead] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
                socketShutdown();
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
                        break;
                    boost::system::error_code read_error;
                    resetTimeout();
                    boost::asio::read(socket_, readBuffer_,
                                      boost::asio::transfer_at_least(1), read_error);
                    cancelTimeout();
                    if (read_error == boost::asio::error::eof) {
                        log_->write(
                                "[" + to_string(uuid_) + "] [TCPConnection handleRead] [EOF] Connection closed by peer.",
                                Log::Level::DEBUG);
                        socketShutdown();
                        return;
                    } else if (read_error) {
                        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection handleRead] [error] ") +
                                            read_error.message(),
                                    Log::Level::ERROR);
                        socketShutdown();
                        return;
                    }
                }
                timer.expires_from_now(
                        boost::posix_time::milliseconds(config_->general().timeWait));
                timer.wait();
            }

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

            if (config_->runMode() == RunMode::agent) {
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
            } else if (config_->runMode() == RunMode::server) {
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
            }
        }
    } catch (std::exception &error) {
        log_->write(
                std::string("[" + to_string(uuid_) + "] [TCPConnection handleRead] [catch read] ") + error.what(),
                Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::doReadRest() {
    try {
        readBuffer_.consume(readBuffer_.size());
        boost::system::error_code error;
        resetTimeout();

        boost::asio::read(socket_, readBuffer_, boost::asio::transfer_at_least(1),
                          error);
        cancelTimeout();

        boost::asio::steady_timer timer(io_context_);
        for (auto i = 0; i <= config_->general().repeatWait; i++) {
            while (true) {
                if (!socket_.is_open()) {
                    log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadRest] Socket is not OPEN",
                                Log::Level::DEBUG);
                    socketShutdown();
                    return;
                }
                if (socket_.available() == 0) break;
                resetTimeout();
                boost::asio::read(socket_, readBuffer_,
                                  boost::asio::transfer_at_least(1), error);
                cancelTimeout();
                if (error == boost::asio::error::eof) {
                    log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [EOF] Connection closed by peer.",
                                Log::Level::TRACE);
                    socketShutdown();
                    return;
                } else if (error) {
                    log_->write(
                            std::string("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [error] ") + error.message(),
                            Log::Level::ERROR);
                    socketShutdown();
                    return;
                }
            }
            timer.expires_after(std::chrono::milliseconds(config_->general().timeWait));
            timer.wait();
        }

        if (readBuffer_.size() > 0) {
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
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPConnection doReadRest] [catch] ") + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::doWrite(auto handlerPointer) {
    boost::system::error_code error;
    try {
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
                    doRead();
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