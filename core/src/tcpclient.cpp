#include "tcpclient.hpp"

#include <openssl/ssl.h>

TCPClient::TCPClient(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(io_context),
      socket_(io_context),
      sslContext_(boost::asio::ssl::context::tls_client),
      sslSocket_(nullptr),
      tlsEnabled_(false),
      resolver_(io_context),
      timeout_(io_context) {
    end_ = false;
}

TCPClient::tcp::socket &TCPClient::socket() { return socket_; }

TCPClient::ssl_stream &TCPClient::sslSocket() { return *sslSocket_; }

bool TCPClient::tlsEnabled() const { return tlsEnabled_; }

bool TCPClient::isOpen() const {
    if (tlsEnabled_ && sslSocket_) return sslSocket_->lowest_layer().is_open();
    return socket_.is_open();
}

bool TCPClient::enableTlsClient() {
    std::lock_guard lock(mutex_);
    try {
        if (tlsEnabled_) return true;

        sslContext_ = boost::asio::ssl::context(boost::asio::ssl::context::tls_client);

        sslContext_.set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3);

        SSL_CTX_set_min_proto_version(sslContext_.native_handle(), TLS1_2_VERSION);

        if (config_->agent().tlsVerifyPeer) {
            sslContext_.set_verify_mode(boost::asio::ssl::verify_peer);
            if (!config_->agent().tlsCaFile.empty()) {
                sslContext_.load_verify_file(config_->agent().tlsCaFile);
            }
        } else {
            sslContext_.set_verify_mode(boost::asio::ssl::verify_none);
        }

        if (SSL_CTX_set_cipher_list(
                    sslContext_.native_handle(),
                    "ECDHE-RSA-AES256-GCM-SHA384:"
                    "ECDHE-RSA-AES128-GCM-SHA256:"
                    "ECDHE-RSA-CHACHA20-POLY1305:"
                    "AES256-GCM-SHA384:"
                    "AES128-GCM-SHA256") != 1) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient enableTlsClient] failed to set cipher list",
                        Log::Level::ERROR);
            return false;
        }

#if defined(TLS1_3_VERSION)
        SSL_CTX_set_ciphersuites(
                sslContext_.native_handle(),
                "TLS_AES_256_GCM_SHA384:"
                "TLS_AES_128_GCM_SHA256:"
                "TLS_CHACHA20_POLY1305_SHA256");
#endif

        sslSocket_ = std::make_unique<ssl_stream>(io_context_, sslContext_);
        tlsEnabled_ = true;
        return true;
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient enableTlsClient] " + error.what(),
                    Log::Level::ERROR);
        return false;
    }
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

        if (error_code) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doConnect] resolve error: " +
                                error_code.message(),
                        Log::Level::ERROR);
            return false;
        }

        if (tlsEnabled_ && sslSocket_) {
            boost::asio::connect(sslSocket_->lowest_layer(), endpoint, error_code);
        } else {
            boost::asio::connect(socket_, endpoint, error_code);
        }

        if (error_code) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doConnect] connect error: " +
                                error_code.message(),
                        Log::Level::ERROR);
            return false;
        }

        return true;
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPClient doConnect] " +
                            error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

bool TCPClient::doHandshakeClient() {
    std::lock_guard lock(mutex_);
    try {
        if (!tlsEnabled_ || !sslSocket_) return true;

        std::string sniHost = config_->general().fakeUrl;

        // If fakeUrl is a full URL (e.g. https://example.com/path),
        // extract only hostname
        if (!sniHost.empty()) {
            // remove scheme if present
            auto pos = sniHost.find("://");
            if (pos != std::string::npos) {
                sniHost = sniHost.substr(pos + 3);
            }

            // remove path if present
            pos = sniHost.find('/');
            if (pos != std::string::npos) {
                sniHost = sniHost.substr(0, pos);
            }

            // remove port if present
            pos = sniHost.find(':');
            if (pos != std::string::npos) {
                sniHost = sniHost.substr(0, pos);
            }

            if (!sniHost.empty()) {
                if (!SSL_set_tlsext_host_name(sslSocket_->native_handle(),
                                              sniHost.c_str())) {
                    log_->write("[" + to_string(uuid_) +
                                        "] [TLS] Failed to set SNI: " + sniHost,
                                Log::Level::ERROR);
                    return false;
                }

                log_->write("[" + to_string(uuid_) +
                                    "] [TLS] Using SNI: " + sniHost,
                            Log::Level::DEBUG);
            }
        }

        resetTimeout();
        sslSocket_->handshake(boost::asio::ssl::stream_base::client);
        cancelTimeout();

        return true;
    } catch (std::exception &error) {
        cancelTimeout();
        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doHandshakeClient] " + error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

void TCPClient::doWrite(boost::asio::streambuf &buffer) {
    std::lock_guard lock(mutex_);
    try {
        moveStreambuf(buffer, writeBuffer_);

        if (!isOpen()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] socket is not open",
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        boost::system::error_code error;
        resetTimeout();

        if (tlsEnabled_ && sslSocket_) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doWrite] [DST " +
                                sslSocket_->lowest_layer().remote_endpoint().address().to_string() +
                                ":" +
                                std::to_string(sslSocket_->lowest_layer().remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            boost::asio::write(*sslSocket_, writeBuffer_, error);
        } else {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doWrite] [DST " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(writeBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
            boost::asio::write(socket_, writeBuffer_, error);
        }

        cancelTimeout();

        if (error) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] [error] " + error.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPClient doWrite] [catch] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPClient::doReadAgent() {
    boost::system::error_code error;
    std::lock_guard lock(mutex_);
    readBuffer_.consume(readBuffer_.size());

    try {
        if (!isOpen()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadAgent] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        resetTimeout();

        if (tlsEnabled_ && sslSocket_) {
            boost::asio::read_until(*sslSocket_, readBuffer_, "COMP\r\n\r\n", error);
        } else {
            boost::asio::read_until(socket_, readBuffer_, "COMP\r\n\r\n", error);
        }

        cancelTimeout();

        if (error == boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadAgent] [EOF] Connection closed by peer.",
                        Log::Level::TRACE);
            socketShutdown();
            return;
        } else if (error) {
            log_->write("[" + to_string(uuid_) +
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
        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doReadAgent] [catch read] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
        return;
    }
}

void TCPClient::doReadServer() {
    boost::system::error_code error;
    end_ = false;
    std::lock_guard lock(mutex_);
    readBuffer_.consume(readBuffer_.size());

    try {
        if (!isOpen()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadServer] Socket is not OPEN",
                        Log::Level::DEBUG);
            return;
        }

        std::array<char, 16384> temp{};
        resetTimeout();

        std::size_t bytes = 0;
        if (tlsEnabled_ && sslSocket_) {
            bytes = sslSocket_->read_some(boost::asio::buffer(temp), error);
        } else {
            bytes = socket_.read_some(boost::asio::buffer(temp), error);
        }

        cancelTimeout();

        if (error == boost::asio::error::eof) {
            socketShutdown();
            return;
        } else if (error) {
            log_->write("[" + to_string(uuid_) +
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

        end_ = false;

        if (tlsEnabled_ && sslSocket_) {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [SRC " +
                                sslSocket_->lowest_layer().remote_endpoint().address().to_string() +
                                ":" +
                                std::to_string(sslSocket_->lowest_layer().remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
        } else {
            log_->write("[" + to_string(uuid_) + "] [TCPClient doReadServer] [SRC " +
                                socket_.remote_endpoint().address().to_string() + ":" +
                                std::to_string(socket_.remote_endpoint().port()) +
                                "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                        Log::Level::DEBUG);
        }

        copyStreambuf(readBuffer_, buffer_);
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
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

    log_->write("[" + to_string(uuid_) +
                        "] [TCPClient onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);
    socketShutdown();
}

void TCPClient::socketShutdown() {
    try {
        boost::system::error_code ignored;

        if (sslSocket_) {
            sslSocket_->shutdown(ignored);
            sslSocket_->lowest_layer().shutdown(
                    boost::asio::ip::tcp::socket::shutdown_both, ignored);
            sslSocket_->lowest_layer().close(ignored);
        }

        if (socket_.is_open()) {
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
            socket_.close(ignored);
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient socketShutdown] [catch] " + error.what(),
                    Log::Level::DEBUG);
    }
}