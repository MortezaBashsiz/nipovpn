#include "tcpclient.hpp"

#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "http_utils.hpp"

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

    try {
        if (tlsEnabled_) return true;

        sslContext_ = boost::asio::ssl::context(boost::asio::ssl::context::tls_client);

        sslContext_.set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3);

        SSL_CTX_set_min_proto_version(sslContext_.native_handle(), TLS1_2_VERSION);

        if (config_->general().tlsVerifyPeer) {
            sslContext_.set_verify_mode(boost::asio::ssl::verify_peer);

            if (!config_->general().tlsCaFile.empty()) {
                sslContext_.load_verify_file(config_->general().tlsCaFile);
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

bool TCPClient::doConnect(const std::string &dstIp, unsigned short dstPort) {
    try {
        if (tlsEnabled_ && sslSocket_ && sslSocket_->lowest_layer().is_open()) {
            return true;
        }

        if (!tlsEnabled_ && socket_.is_open()) {
            return true;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doConnect] [DST " + dstIp + ":" +
                            std::to_string(dstPort) + "]",
                    Log::Level::DEBUG);

        boost::system::error_code error_code;

        auto endpoints = resolver_.resolve(
                dstIp,
                std::to_string(dstPort),
                error_code);

        if (error_code) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doConnect] resolve error: " +
                                error_code.message(),
                        Log::Level::ERROR);
            return false;
        }

        resetTimeout();

        if (tlsEnabled_) {
            sslSocket_ = std::make_unique<ssl_stream>(io_context_, sslContext_);
            closed_ = false;

            boost::asio::connect(
                    sslSocket_->lowest_layer(),
                    endpoints,
                    error_code);
        } else {
            socket_ = tcp::socket(io_context_);
            closed_ = false;

            boost::asio::connect(
                    socket_,
                    endpoints,
                    error_code);
        }

        cancelTimeout();

        if (error_code) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doConnect] connect error: " +
                                error_code.message(),
                        Log::Level::ERROR);
            socketShutdown();
            return false;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doConnect] connected",
                    Log::Level::DEBUG);

        return true;

    } catch (const std::exception &ex) {
        cancelTimeout();

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doConnect] exception: " + ex.what(),
                    Log::Level::ERROR);

        socketShutdown();
        return false;
    }
}

bool TCPClient::doHandshakeClient() {
    try {
        if (!tlsEnabled_ || !sslSocket_) {
            return true;
        }

        if (!sslSocket_->lowest_layer().is_open()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doHandshakeClient] socket is not open",
                        Log::Level::ERROR);
            socketShutdown();
            return false;
        }

        std::string sniHost = config_->randomFakeUrl();

        auto pos = sniHost.find("://");
        if (pos != std::string::npos) {
            sniHost = sniHost.substr(pos + 3);
        }

        pos = sniHost.find('/');
        if (pos != std::string::npos) {
            sniHost = sniHost.substr(0, pos);
        }

        pos = sniHost.find(':');
        if (pos != std::string::npos) {
            sniHost = sniHost.substr(0, pos);
        }

        if (!sniHost.empty()) {
            if (!SSL_set_tlsext_host_name(
                        sslSocket_->native_handle(),
                        sniHost.c_str())) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TLS] Failed to set SNI: " + sniHost,
                            Log::Level::ERROR);
                socketShutdown();
                return false;
            }

            static const unsigned char alpn[] = {
                    0x08, 'h', 't', 't', 'p', '/', '1', '.', '1'};

            if (SSL_set_alpn_protos(
                        sslSocket_->native_handle(),
                        alpn,
                        sizeof(alpn)) != 0) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TLS] Failed to set ALPN http/1.1",
                            Log::Level::ERROR);
                socketShutdown();
                return false;
            }

            log_->write("[" + to_string(uuid_) +
                                "] [TLS] Starting client handshake with SNI: " +
                                sniHost,
                        Log::Level::DEBUG);
        }

        boost::system::error_code error;
        resetTimeout();

        sslSocket_->handshake(
                boost::asio::ssl::stream_base::client,
                error);

        cancelTimeout();

        if (error) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doHandshakeClient] handshake error: " +
                                error.message(),
                        Log::Level::ERROR);
            socketShutdown();
            return false;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TLS] Client handshake completed",
                    Log::Level::DEBUG);

        const unsigned char *selected = nullptr;
        unsigned int selectedLen = 0;

        SSL_get0_alpn_selected(
                sslSocket_->native_handle(),
                &selected,
                &selectedLen);

        if (selected != nullptr && selectedLen > 0) {
            std::string negotiated(
                    reinterpret_cast<const char *>(selected),
                    selectedLen);

            log_->write("[" + to_string(uuid_) +
                                "] [TLS] Negotiated ALPN: " + negotiated,
                        Log::Level::DEBUG);
        } else {
            log_->write("[" + to_string(uuid_) +
                                "] [TLS] No ALPN negotiated",
                        Log::Level::DEBUG);
        }

        return true;

    } catch (const std::exception &ex) {
        cancelTimeout();

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doHandshakeClient] exception: " +
                            ex.what(),
                    Log::Level::ERROR);

        socketShutdown();
        return false;
    }
}

void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    moveStreambuf(buffer, writeBuffer_);
}


bool TCPClient::doWrite(boost::asio::streambuf &buffer) {
    try {
        writeBuffer_.consume(writeBuffer_.size());
        moveStreambuf(buffer, writeBuffer_);

        if (!isOpen()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] socket is not open",
                        Log::Level::DEBUG);
            socketShutdown();
            return false;
        }

        if (writeBuffer_.size() == 0) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] empty write buffer",
                        Log::Level::DEBUG);
            socketShutdown();
            return false;
        }

        boost::system::error_code error;
        resetTimeout();

        if (tlsEnabled_ && sslSocket_) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] [SSL] [Bytes " +
                                std::to_string(writeBuffer_.size()) + "]",
                        Log::Level::DEBUG);

            boost::asio::write(*sslSocket_, writeBuffer_, error);
        } else {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] [TCP] [Bytes " +
                                std::to_string(writeBuffer_.size()) + "]",
                        Log::Level::DEBUG);

            boost::asio::write(socket_, writeBuffer_, error);
        }

        cancelTimeout();

        if (error) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doWrite] [error] " + error.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return false;
        }

        return true;

    } catch (const std::exception &ex) {
        cancelTimeout();

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doWrite] [catch] " + ex.what(),
                    Log::Level::DEBUG);

        socketShutdown();
        return false;
    }
}

bool TCPClient::doReadAgent() {
    boost::system::error_code error;

    try {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            readBuffer_.consume(readBuffer_.size());
            buffer_.consume(buffer_.size());

            if (!isOpen()) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPClient doReadAgent] Socket is not OPEN",
                            Log::Level::DEBUG);
                return false;
            }
        }

        resetTimeout();

        bool ok = false;
        if (tlsEnabled_) {
            if (!sslSocket_) {
                cancelTimeout();
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPClient doReadAgent] SSL socket is null",
                            Log::Level::DEBUG);
                socketShutdown();
                return false;
            }

            ok = HttpUtils::readHttpMessage(*sslSocket_, readBuffer_, error);
        } else {
            ok = HttpUtils::readHttpMessage(socket_, readBuffer_, error);
        }

        cancelTimeout();

        if (!ok) {
            if (error == boost::asio::error::operation_aborted) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPClient doReadAgent] [aborted]",
                            Log::Level::DEBUG);
            } else if (error == boost::asio::error::eof) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPClient doReadAgent] [EOF] Connection closed by peer.",
                            Log::Level::TRACE);
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPClient doReadAgent] [error] " + error.message(),
                            Log::Level::ERROR);
            }

            socketShutdown();
            return false;
        }

        if (readBuffer_.size() == 0) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPClient doReadAgent] empty read buffer",
                        Log::Level::DEBUG);
            socketShutdown();
            return false;
        }

        copyStreambuf(readBuffer_, buffer_);
        return true;

    } catch (const std::exception &ex) {
        cancelTimeout();

        log_->write("[" + to_string(uuid_) +
                            "] [TCPClient doReadAgent] [catch read] " + ex.what(),
                    Log::Level::DEBUG);

        socketShutdown();
        return false;
    }
}

void TCPClient::doReadServer() {
    boost::system::error_code error;
    end_ = false;
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
    if (error == boost::asio::error::operation_aborted) {
        return;
    }

    log_->write("[" + to_string(uuid_) +
                        "] [TCPClient onTimeout] timeout expired",
                Log::Level::TRACE);

    socketShutdown();
}

void TCPClient::socketShutdown() {
    boost::system::error_code ec;

    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_) return;
    closed_ = true;

    cancelTimeout();

    if (sslSocket_) {
        log_->write("[" + to_string(uuid_) + "] [TCPClient socketShutdown] [SSL]",
                    Log::Level::DEBUG);

        auto &lowest = sslSocket_->lowest_layer();

        lowest.cancel(ec);
        ec.clear();

        if (lowest.is_open()) {
            lowest.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            ec.clear();

            lowest.close(ec);
        }
    }

    if (socket_.is_open()) {
        log_->write("[" + to_string(uuid_) + "] [TCPClient socketShutdown] [TCP]",
                    Log::Level::DEBUG);

        socket_.cancel(ec);
        ec.clear();

        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        ec.clear();

        socket_.close(ec);
    }

    tlsEnabled_ = false;
}

std::string TCPClient::HttpUtils::toLowerCopy(std::string s) {
    return http_utils::toLowerCopy(std::move(s));
}

std::string TCPClient::HttpUtils::extractHeaders(const std::string &msg) {
    return http_utils::extractHeaders(msg);
}

std::string TCPClient::HttpUtils::extractBody(const std::string &msg) {
    return http_utils::extractBody(msg);
}

bool TCPClient::HttpUtils::parseContentLength(const std::string &headers, std::size_t &value) {
    return http_utils::parseContentLength(headers, value);
}

bool TCPClient::HttpUtils::isChunked(const std::string &headers) {
    return http_utils::isChunked(headers);
}

bool TCPClient::HttpUtils::readHttpMessage(
        tcp::socket &stream,
        boost::asio::streambuf &out,
        boost::system::error_code &ec) {
    return readHttpMessageImpl(
            out,
            ec,
            [&stream](boost::asio::mutable_buffer buffer,
                      boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            },
            [&stream](boost::asio::streambuf &buffer,
                      boost::system::error_code &error) {
                boost::asio::read_until(stream, buffer, "\r\n\r\n", error);
                return !error;
            });
}

bool TCPClient::HttpUtils::readHttpMessage(
        ssl_stream &stream,
        boost::asio::streambuf &out,
        boost::system::error_code &ec) {
    return readHttpMessageImpl(
            out,
            ec,
            [&stream](boost::asio::mutable_buffer buffer,
                      boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            },
            [&stream](boost::asio::streambuf &buffer,
                      boost::system::error_code &error) {
                boost::asio::read_until(stream, buffer, "\r\n\r\n", error);
                return !error;
            });
}

bool TCPClient::HttpUtils::readHttpMessageImpl(
        boost::asio::streambuf &out,
        boost::system::error_code &ec,
        const std::function<std::size_t(
                boost::asio::mutable_buffer,
                boost::system::error_code &)> &readSome,
        const std::function<bool(
                boost::asio::streambuf &,
                boost::system::error_code &)> &readHeaders) {

    out.consume(out.size());

    if (!readHeaders(out, ec)) {
        return false;
    }

    std::string current = streambufToString(out);
    std::string headers = extractHeaders(current);
    std::string body = extractBody(current);

    std::size_t contentLength = 0;
    if (parseContentLength(headers, contentLength)) {
        while (body.size() < contentLength) {
            char tmp[8192];
            std::size_t remaining =
                    contentLength - body.size();
            std::size_t toRead =
                    std::min<std::size_t>(
                            sizeof(tmp),
                            remaining);
            std::size_t n =
                    readSome(
                            boost::asio::buffer(
                                    tmp,
                                    toRead),
                            ec);
            if (ec) {
                return false;
            }
            if (n == 0) {
                ec = boost::asio::error::eof;
                return false;
            }
            boost::asio::buffer_copy(
                    out.prepare(n),
                    boost::asio::buffer(tmp, n));
            out.commit(n);
            body.append(tmp, n);
        }
        return true;
    }
    if (isChunked(headers)) {
        while (
                body.find("\r\n0\r\n\r\n") ==
                        std::string::npos &&
                body.find("\r\n0\r\n") ==
                        std::string::npos) {
            char tmp[8192];
            std::size_t n =
                    readSome(
                            boost::asio::buffer(tmp),
                            ec);
            if (ec) {
                return false;
            }
            if (n == 0) {
                ec = boost::asio::error::eof;
                return false;
            }
            boost::asio::buffer_copy(
                    out.prepare(n),
                    boost::asio::buffer(tmp, n));
            out.commit(n);
            body.append(tmp, n);
        }
        return true;
    }
    return true;
}
