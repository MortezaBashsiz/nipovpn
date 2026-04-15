#include "tcpclient.hpp"

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

boost::asio::ip::tcp::socket &TCPClient::socket() {
    std::lock_guard lock(mutex_);
    return socket_;
}

void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

bool TCPClient::doConnect(const std::string &dstIP, const unsigned short &dstPort) {
    std::lock_guard lock(mutex_);
    try {
        log_->write("[" + to_string(uuid_) + "] [TCPClient doConnect] [DST " + dstIP +
                            ":" + std::to_string(dstPort) + "]",
                    Log::Level::DEBUG);

        boost::system::error_code error_code;
        auto endpoint =
                resolver_.resolve(dstIP.c_str(), std::to_string(dstPort).c_str(), error_code);

        if (error_code) return false;

        boost::asio::connect(socket_, endpoint);
        return true;
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPClient doConnect] " + error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

void TCPClient::doWrite(boost::asio::streambuf &buffer) {
    std::lock_guard lock(mutex_);
    try {
        moveStreambuf(buffer, writeBuffer_);

        if (!socket_.is_open()) {
            socketShutdown();
            return;
        }

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();
        boost::system::error_code error;
        boost::asio::write(socket_, writeBuffer_, error);
        cancelTimeout();

        if (error) {
            log_->write(std::string("[") + to_string(uuid_) +
                                "] [TCPClient doWrite] [error] " + error.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPClient doWrite] [catch] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPClient::doReadAgent() {
    boost::system::error_code error;
    std::lock_guard lock(mutex_);
    readBuffer_.consume(readBuffer_.size());

    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadAgent] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();
        boost::asio::read_until(socket_, readBuffer_, "COMP\r\n\r\n", error);
        cancelTimeout();

        if (error == boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadAgent] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
            socketShutdown();
            return;
        } else if (error) {
            log_->write(std::string("[") + to_string(uuid_) +
                                "] [TCPClient doReadAgent] [error] " + error.message(),
                        Log::Level::ERROR);
            socketShutdown();
            return;
        }

        if (readBuffer_.size() > 0) {
            copyStreambuf(readBuffer_, buffer_);
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPClient doReadAgent] [catch read] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPClient::doReadServer() {
    boost::system::error_code error;
    std::lock_guard lock(mutex_);
    readBuffer_.consume(readBuffer_.size());
    end_ = false;

    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadServer] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        std::array<char, 16384> temp{};
        resetTimeout();
        const auto bytes = socket_.read_some(boost::asio::buffer(temp), error);
        cancelTimeout();

        if (error == boost::asio::error::eof) {
            socketShutdown();
            return;
        } else if (error) {
            log_->write(std::string("[") + to_string(uuid_) +
                                "] [TCPClient doReadServer] [error] " + error.message(),
                        Log::Level::ERROR);
            socketShutdown();
            return;
        }

        if (bytes == 0) {
            end_ = true;
            return;
        }

        std::ostream os(&readBuffer_);
        os.write(temp.data(), static_cast<std::streamsize>(bytes));

        // In pipe mode, every successful read is a forwardable chunk.
        // Do not guess "end of message" here.
        end_ = false;

        log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        copyStreambuf(readBuffer_, buffer_);
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPClient doReadServer] [catch read] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPClient::resetTimeout() {
    if (!config_->general().timeout) return;

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));
    timeout_.async_wait(
            [self = shared_from_this()](const boost::system::error_code &error) {
                self->onTimeout(error);
            });
}

void TCPClient::cancelTimeout() {
    if (config_->general().timeout) timeout_.cancel();
}

void TCPClient::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;

    log_->write("[" + to_string(uuid_) + "] [TCPClient onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);
    socketShutdown();
}

void TCPClient::socketShutdown() {
    try {
        boost::system::error_code ignored;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
    } catch (std::exception &error) {
        log_->write(std::string("[") + to_string(uuid_) +
                            "] [TCPClient socketShutdown] [catch] " + error.what(),
                    Log::Level::DEBUG);
    }
}