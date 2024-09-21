#include "tcpclient.hpp"

#include <boost/asio/steady_timer.hpp>

TCPClient::TCPClient(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(io_context),
      socket_(io_context),
      resolver_(io_context),
      timeout_(io_context) {}

TCPClient::pointer TCPClient::create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log)
{
    return pointer(new TCPClient(io_context, config, log));
}

boost::asio::ip::tcp::socket &TCPClient::getSocket() {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_;
}

void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

boost::asio::streambuf& TCPClient::getWriteBuffer() & {
    return writeBuffer_;
}

boost::asio::streambuf&& TCPClient::getWriteBuffer() && {
    return std::move(writeBuffer_);
}

boost::asio::streambuf& TCPClient::getReadBuffer() & {
    return readBuffer_;
}

boost::asio::streambuf&& TCPClient::getReadBuffer() && {
    return std::move(readBuffer_);
}

bool TCPClient::doConnect(const std::string &dstIP,
                          const unsigned short &dstPort) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Log connection attempt
        log_->write("[" + to_string(uuid_) + "] [TCPClient doConnect] [DST " + dstIP + ":" +
                            std::to_string(dstPort) + "]",
                    Log::Level::DEBUG);

        // Resolve the endpoint and connect
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

void TCPClient::doWrite(boost::asio::streambuf &buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        moveStreambuf(buffer, writeBuffer_);

        // Log details of the write operation
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

        // Perform the write operation
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

void TCPClient::doRead() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        readBuffer_.consume(readBuffer_.size());

        boost::system::error_code error;
        resetTimeout();

        // Read at least 39 bytes from the socket
        boost::asio::read(socket_, readBuffer_, boost::asio::transfer_at_least(1),
                          error);

        cancelTimeout();

        // Implement retry mechanism if data is still available
        boost::asio::steady_timer timer(io_context_);
        for (auto i = 0; i <= config_->general().repeatWait; i++) {
            while (true) {
                if (!socket_.is_open()) {
                    log_->write("[" + to_string(uuid_) + "] [TCPClient doRead] Socket is not OPEN",
                                Log::Level::DEBUG);
                    socketShutdown();
                    return;
                }

                if (socket_.available() == 0) break;

                resetTimeout();

                boost::asio::read(socket_, readBuffer_, boost::asio::transfer_at_least(1),
                        error);

                cancelTimeout();

                if (error == boost::asio::error::eof) {
                    log_->write("[" + to_string(uuid_) + "] [TCPClient doRead] [EOF] Connection closed by peer.",
                                Log::Level::TRACE);
                    socketShutdown();
                    return;
                } else if (error) {
                    log_->write(
                            std::string("[" + to_string(uuid_) + "] [TCPClient doRead] [error] ") + error.message(),
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
                // Log the successful read operation
                log_->write("[" + to_string(uuid_) + "] [TCPClient doRead] [SRC " +
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
                // Log exceptions during logging
                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPClient doRead] [catch log] ") + error.what(),
                        Log::Level::DEBUG);
            }
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        // Log exceptions during the read operation
        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doRead] [catch read] ") + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
        return;
    }
}

void TCPClient::resetTimeout() {
    if (config_->general().timeout == 0)
        return;

    // Start or restart the timer and cancel old handlers
    timeout_.expires_from_now(boost::posix_time::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPClient::onTimeout,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
}

void TCPClient::cancelTimeout() {
    if (config_->general().timeout != 0)
        timeout_.cancel();
}

void TCPClient::onTimeout(const boost::system::error_code &error) {
    // Ignore cancellation and only handle timer expiration.
    if (error || error == boost::asio::error::operation_aborted) return;

    // Timeout has expired, do necessary actions
    log_->write(
            std::string("[" + to_string(uuid_) + "] [TCPClient onTimeout] [expiration] ") +
                    std::to_string(+config_->general().timeout) +
                    " seconds has passed, and the timeout has expired",
            Log::Level::TRACE);
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

void TCPClient::setUuid(const boost::uuids::uuid& uuid) {
    uuid_ = uuid;
}

boost::uuids::uuid TCPClient::getUuid() const {
    return uuid_;
}