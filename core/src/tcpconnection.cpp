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
    tunnelMode_ = false;
}

boost::asio::ip::tcp::socket &TCPConnection::socket() { return socket_; }

void TCPConnection::startAgent() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadAgent();
}

void TCPConnection::startServer() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadServer();
}

void TCPConnection::doReadAgent() {
    try {
        if (tunnelMode_) {
            relayClientToRemote();
            return;
        }

        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());

        resetTimeout();
        boost::asio::async_read(
                socket_, readBuffer_, boost::asio::transfer_exactly(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadAgent, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPConnection doReadAgent] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::doReadServer() {
    try {
        if (tunnelMode_) {
            relayClientToRemote();
            return;
        }

        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());

        resetTimeout();
        boost::asio::async_read_until(
                socket_, readBuffer_, "COMP\r\n\r\n",
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadServer, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPConnection doReadServer] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleReadAgent(const boost::system::error_code &error,
                                    size_t) {
    try {
        cancelTimeout();

        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadAgent] [EOF] "
                                    "Connection closed by peer.",
                            Log::Level::TRACE);
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadAgent] " + error.message(),
                            Log::Level::TRACE);
            }
            socketShutdown();
            return;
        }

        // Keep the old initial-request completion logic only for the very first parse.
        boost::system::error_code errorIn;
        std::string bufStr{hexStreambufToStr(readBuffer_)};

        if (bufStr == "43") {
            boost::asio::read_until(socket_, readBuffer_, "\r\n", errorIn);
        } else {
            boost::asio::read_until(socket_, readBuffer_, "\r\n\r\n", errorIn);
        }

        if (errorIn && errorIn != boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection handleReadAgent] [errorIn] " +
                                errorIn.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] "
                            "[SRC " +
                            socket_.remote_endpoint().address().to_string() +
                            ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        AgentHandler::pointer agentHandler_ = AgentHandler::create(
                readBuffer_, writeBuffer_, config_, log_, client_,
                socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()),
                uuid_);

        agentHandler_->handle();

        end_ = agentHandler_->end_;
        connect_ = agentHandler_->connect_;

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();
        boost::asio::async_write(
                socket_, writeBuffer_.data(),
                boost::asio::bind_executor(
                        strand_,
                        [self = shared_from_this()](const boost::system::error_code &ec,
                                                    std::size_t) {
                            self->cancelTimeout();
                            if (ec) {
                                self->log_->write("[" + to_string(self->uuid_) +
                                                          "] [TCPConnection write agent] " +
                                                          ec.message(),
                                                  Log::Level::ERROR);
                                self->socketShutdown();
                                return;
                            }

                            if (self->connect_) {
                                self->enableTunnelMode();
                                self->relayClientToRemote();
                                self->relayRemoteToClient();
                                return;
                            }

                            self->doReadAgent();
                        }));
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::handleReadServer(const boost::system::error_code &error,
                                     size_t) {
    try {
        cancelTimeout();

        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] [EOF] "
                                    "Connection closed by peer.",
                            Log::Level::TRACE);
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] " +
                                    error.message(),
                            Log::Level::TRACE);
            }
            socketShutdown();
            return;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] "
                            "[SRC " +
                            socket_.remote_endpoint().address().to_string() +
                            ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        ServerHandler::pointer serverHandler_ = ServerHandler::create(
                readBuffer_, writeBuffer_, config_, log_, client_,
                socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()),
                uuid_);

        serverHandler_->handle();

        end_ = serverHandler_->end_;
        connect_ = serverHandler_->connect_;

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();
        boost::asio::async_write(
                socket_, writeBuffer_.data(),
                boost::asio::bind_executor(
                        strand_,
                        [self = shared_from_this()](const boost::system::error_code &ec,
                                                    std::size_t) {
                            self->cancelTimeout();
                            if (ec) {
                                self->log_->write("[" + to_string(self->uuid_) +
                                                          "] [TCPConnection write server] " +
                                                          ec.message(),
                                                  Log::Level::ERROR);
                                self->socketShutdown();
                                return;
                            }

                            if (self->connect_) {
                                self->enableTunnelMode();
                                self->relayClientToRemote();
                                self->relayRemoteToClient();
                                return;
                            }

                            self->doReadServer();
                        }));
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::enableTunnelMode() {
    tunnelMode_ = true;
}

void TCPConnection::relayClientToRemote() {
    if (!socket_.is_open() || !client_->socket().is_open()) {
        socketShutdown();
        return;
    }

    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec,
                                                std::size_t bytes) {
                        if (ec) {
                            self->socketShutdown();
                            return;
                        }

                        boost::asio::async_write(
                                self->client_->socket(),
                                boost::asio::buffer(self->downstreamBuf_.data(), bytes),
                                boost::asio::bind_executor(
                                        self->strand_,
                                        [self](const boost::system::error_code &werr, std::size_t) {
                                            if (werr) {
                                                self->socketShutdown();
                                                return;
                                            }
                                            self->relayClientToRemote();
                                        }));
                    }));
}

void TCPConnection::relayRemoteToClient() {
    if (!socket_.is_open() || !client_->socket().is_open()) {
        socketShutdown();
        return;
    }

    client_->socket().async_read_some(
            boost::asio::buffer(upstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec,
                                                std::size_t bytes) {
                        if (ec) {
                            self->socketShutdown();
                            return;
                        }

                        boost::asio::async_write(
                                self->socket_,
                                boost::asio::buffer(self->upstreamBuf_.data(), bytes),
                                boost::asio::bind_executor(
                                        self->strand_,
                                        [self](const boost::system::error_code &werr, std::size_t) {
                                            if (werr) {
                                                self->socketShutdown();
                                                return;
                                            }
                                            self->relayRemoteToClient();
                                        }));
                    }));
}

void TCPConnection::resetTimeout() {
    if (!config_->general().timeout) return;

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPConnection::onTimeout, shared_from_this(),
                                    boost::asio::placeholders::error));
}

void TCPConnection::cancelTimeout() {
    if (config_->general().timeout) timeout_.cancel();
}

void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;

    log_->write("[" + to_string(uuid_) +
                        "] [TCPConnection onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);
    socketShutdown();
}

void TCPConnection::socketShutdown() {
    try {
        boost::system::error_code ignored;

        if (socket_.is_open()) {
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
            socket_.close(ignored);
        }

        if (client_->socket().is_open()) {
            client_->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                       ignored);
            client_->socket().close(ignored);
        }
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPConnection socketShutdown] [catch] " +
                            error.what(),
                    Log::Level::DEBUG);
    }
}