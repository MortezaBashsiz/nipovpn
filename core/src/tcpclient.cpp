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
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_;
}

void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

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

void TCPClient::doReadAgent() {
    boost::system::error_code error;
    std::lock_guard<std::mutex> lock(mutex_);
    boost::asio::streambuf tempBuff;

    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadAgent] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();
        boost::asio::read_until(socket_, tempBuff, "COMP\r\n\r\n",
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

        if (tempBuff.size() > 0) {
            copyStreambuf(tempBuff, buffer_);
            try {

                log_->write("[" + to_string(uuid_) + "] [TCPClient doReadAgent] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] [Bytes " + std::to_string(tempBuff.size()) + "] ",
                            Log::Level::DEBUG);
                log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] " + "[Bytes " + std::to_string(tempBuff.size()) +
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

void TCPClient::doReadServer() {
    boost::system::error_code error;
    end_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    boost::asio::streambuf tempBuff;
    boost::asio::steady_timer timer(io_context_);
    bool isTlsRecord{false};

    try {
        if (!socket_.is_open()) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();
        boost::asio::read(socket_, tempBuff, boost::asio::transfer_exactly(2),
                          error);
        std::string bufStr{hexStreambufToStr(tempBuff)};
        if (bufStr == "1403" || bufStr == "1503" || bufStr == "1603" || bufStr == "1703") {
            isTlsRecord = true;
            boost::asio::streambuf tempTempBuff;
            boost::asio::read(socket_, tempBuff, boost::asio::transfer_exactly(1),
                              error);
            boost::asio::read(socket_, tempTempBuff, boost::asio::transfer_exactly(2),
                              error);
            int readSize{hexToInt(hexStreambufToStr(tempTempBuff))};
            moveStreambuf(tempTempBuff, tempBuff);
            boost::asio::read(socket_, tempBuff, boost::asio::transfer_exactly(readSize),
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

        if (!isTlsRecord) {
            for (auto i = 0; i <= config_->general().repeatWait; i++) {
                while (true) {
                    if (socket_.available() == 0) break;
                    resetTimeout();
                    boost::asio::read(socket_, tempBuff,
                                      boost::asio::transfer_at_least(1), error);
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
                }
                timer.expires_after(std::chrono::milliseconds(config_->general().timeWait));
                timer.wait();
            }
        }


        timer.expires_after(std::chrono::milliseconds(config_->general().timeWait));
        timer.wait();
        if (socket_.available() == 0) {
            end_ = true;
        }

        if (tempBuff.size() > 0) {
            copyStreambuf(tempBuff, buffer_);
            try {

                log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] [Bytes " + std::to_string(tempBuff.size()) + "] ",
                            Log::Level::DEBUG);
                log_->write("[" + to_string(uuid_) + "] [Read from] [SRC " +
                                    socket_.remote_endpoint().address().to_string() + ":" +
                                    std::to_string(socket_.remote_endpoint().port()) +
                                    "] " + "[Bytes " + std::to_string(tempBuff.size()) +
                                    "] ",
                            Log::Level::TRACE);
            } catch (std::exception &error) {

                log_->write(
                        std::string("[" + to_string(uuid_) + "] [TCPClient doReadServer] [catch log] ") + error.what(),
                        Log::Level::DEBUG);
            }
        } else {
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {

        log_->write(std::string("[" + to_string(uuid_) + "] [TCPClient doReadServer] [catch read] ") + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
        return;
    }
}

void TCPClient::doHandle() {
    buffer_.consume(buffer_.size());
    readBuffer_.consume(readBuffer_.size());
    if (config_->runMode() == RunMode::server) {
        doReadServer();
    } else {
        doReadAgent();
    }
    copyStreambuf(buffer_, readBuffer_);
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