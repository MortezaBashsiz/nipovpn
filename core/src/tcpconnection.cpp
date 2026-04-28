#include "tcpconnection.hpp"

#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

    std::string toLowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::string extractHeaders(const std::string &msg) {
        auto pos = msg.find("\r\n\r\n");
        if (pos == std::string::npos) return {};
        return msg.substr(0, pos + 4);
    }

    std::string extractBody(const std::string &msg) {
        auto pos = msg.find("\r\n\r\n");
        if (pos == std::string::npos) return {};
        return msg.substr(pos + 4);
    }

    bool parseContentLength(const std::string &headers, std::size_t &value) {
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto lower = toLowerCopy(line);
            if (lower.rfind("content-length:", 0) == 0) {
                auto raw = line.substr(std::string("Content-Length:").size());
                raw.erase(0, raw.find_first_not_of(" \t"));
                try {
                    value = static_cast<std::size_t>(std::stoull(raw));
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }
        return false;
    }

    bool isChunked(const std::string &headers) {
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto lower = toLowerCopy(line);
            if (lower.rfind("transfer-encoding:", 0) == 0 &&
                lower.find("chunked") != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    template<typename Stream>
    bool readRemainingHttpBody(Stream &stream,
                               boost::asio::streambuf &buf,
                               boost::system::error_code &ec) {
        std::string current = streambufToString(buf);
        std::string headers = extractHeaders(current);
        std::string body = extractBody(current);

        std::size_t contentLength = 0;
        if (parseContentLength(headers, contentLength)) {
            if (body.size() < contentLength) {
                boost::asio::read(stream, buf,
                                  boost::asio::transfer_exactly(contentLength - body.size()),
                                  ec);
                if (ec) return false;
            }
            return true;
        }

        if (isChunked(headers)) {
            for (;;) {
                current = streambufToString(buf);
                auto pos = current.find("\r\n\r\n");
                if (pos == std::string::npos) return false;
                auto bodyPart = current.substr(pos + 4);

                if (bodyPart.find("\r\n0\r\n\r\n") != std::string::npos ||
                    bodyPart == "0\r\n\r\n") {
                    return true;
                }

                std::array<char, 4096> tmp{};
                std::size_t n = stream.read_some(boost::asio::buffer(tmp), ec);
                if (ec) return false;
                std::ostream os(&buf);
                os.write(tmp.data(), static_cast<std::streamsize>(n));
            }
        }

        return true;
    }

}// namespace


TCPConnection::TCPConnection(boost::asio::io_context &io_context,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             TCPClient::pointer client)
    : socket_(io_context),
      sslContext_(boost::asio::ssl::context::tls_server),
      tlsSocket_(nullptr),
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

TCPConnection::ssl_stream &TCPConnection::tlsSocket() { return *tlsSocket_; }

bool TCPConnection::initTlsServerContext() {
    try {
        sslContext_ = boost::asio::ssl::context(boost::asio::ssl::context::tls_server);

        sslContext_.set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use);

        SSL_CTX_set_min_proto_version(sslContext_.native_handle(), TLS1_2_VERSION);

        sslContext_.use_certificate_chain_file(config_->server().tlsCertFile);
        sslContext_.use_private_key_file(config_->server().tlsKeyFile,
                                         boost::asio::ssl::context::pem);

        if (!SSL_CTX_check_private_key(sslContext_.native_handle())) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection initTlsServerContext] private key does not match certificate",
                        Log::Level::ERROR);
            return false;
        }

        sslContext_.set_verify_mode(boost::asio::ssl::verify_none);

        if (SSL_CTX_set_cipher_list(
                    sslContext_.native_handle(),
                    "ECDHE-RSA-AES256-GCM-SHA384:"
                    "ECDHE-RSA-AES128-GCM-SHA256:"
                    "ECDHE-RSA-CHACHA20-POLY1305:"
                    "AES256-GCM-SHA384:"
                    "AES128-GCM-SHA256") != 1) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection initTlsServerContext] failed to set cipher list",
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

        tlsSocket_ = std::make_unique<ssl_stream>(io_context_, sslContext_);
        return true;

    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection initTlsServerContext] " + error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

bool TCPConnection::doHandshakeServer() {
    try {
        resetTimeout();
        tlsSocket_->handshake(boost::asio::ssl::stream_base::server);
        cancelTimeout();
        return true;
    } catch (std::exception &error) {
        cancelTimeout();
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doHandshakeServer] " + error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

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
                socket_, readBuffer_, boost::asio::transfer_at_least(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadAgent, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
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

        if (config_->server().tlsEnable) {
            boost::asio::async_read_until(
                    *tlsSocket_, readBuffer_, "\r\n\r\n",
                    boost::asio::bind_executor(
                            strand_,
                            boost::bind(&TCPConnection::handleReadServer, shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred)));
        } else {
            boost::asio::async_read_until(
                    socket_, readBuffer_, "\r\n\r\n",
                    boost::asio::bind_executor(
                            strand_,
                            boost::bind(&TCPConnection::handleReadServer, shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred)));
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doReadServer] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleReadAgent(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();

        if (error) {
            if (error == boost::asio::error::eof) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadAgent] [EOF] Connection closed by peer.",
                            Log::Level::TRACE);
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadAgent] " + error.message(),
                            Log::Level::TRACE);
            }
            socketShutdown();
            return;
        }

        boost::system::error_code errorIn;

        std::string current = streambufToString(readBuffer_);
        if (current.find("\r\n\r\n") == std::string::npos) {
            boost::asio::read_until(socket_, readBuffer_, "\r\n\r\n", errorIn);
        }

        if (errorIn && errorIn != boost::asio::error::eof) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection handleReadAgent] [errorIn] " + errorIn.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }

        current = streambufToString(readBuffer_);
        const std::string headers = extractHeaders(current);
        const std::string body = extractBody(current);

        const bool isConnect =
                headers.rfind("CONNECT ", 0) == 0 || headers.rfind("connect ", 0) == 0;

        if (!isConnect) {
            std::size_t contentLength = 0;
            if (parseContentLength(headers, contentLength) && body.size() < contentLength) {
                boost::asio::read(socket_,
                                  readBuffer_,
                                  boost::asio::transfer_exactly(contentLength - body.size()),
                                  errorIn);

                if (errorIn && errorIn != boost::asio::error::eof) {
                    log_->write("[" + to_string(uuid_) +
                                        "] [TCPConnection handleReadAgent] [body read] " +
                                        errorIn.message(),
                                Log::Level::DEBUG);
                    socketShutdown();
                    return;
                }
            }
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] "
                            "[SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] [Bytes " +
                            std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        AgentHandler::pointer agentHandler_ = AgentHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
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
                socket_,
                writeBuffer_.data(),
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
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] [catch read] " + error.what(),
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
                                    "] [TCPConnection handleReadServer] [EOF] Connection closed by peer.",
                            Log::Level::TRACE);
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] " + error.message(),
                            Log::Level::TRACE);
            }
            socketShutdown();
            return;
        }

        boost::system::error_code bodyError;
        std::string remotePeer;

        if (config_->server().tlsEnable) {
            if (!readRemainingHttpBody(*tlsSocket_, readBuffer_, bodyError)) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] body read failed: " +
                                    bodyError.message(),
                            Log::Level::ERROR);
                socketShutdown();
                return;
            }

            remotePeer = tlsSocket_->lowest_layer().remote_endpoint().address().to_string() +
                         ":" +
                         std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port());
        } else {
            if (!readRemainingHttpBody(socket_, readBuffer_, bodyError)) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] body read failed: " +
                                    bodyError.message(),
                            Log::Level::ERROR);
                socketShutdown();
                return;
            }

            remotePeer = socket_.remote_endpoint().address().to_string() + ":" +
                         std::to_string(socket_.remote_endpoint().port());
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] "
                            "[SRC " +
                            remotePeer + "] [Bytes " +
                            std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        ServerHandler::pointer serverHandler_ =
                ServerHandler::create(readBuffer_, writeBuffer_, config_, log_, client_,
                                      remotePeer, uuid_);

        serverHandler_->handle();

        end_ = serverHandler_->end_;
        connect_ = serverHandler_->connect_;

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();

        auto writeHandler =
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
                };

        if (config_->server().tlsEnable) {
            boost::asio::async_write(
                    *tlsSocket_, writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        } else {
            boost::asio::async_write(
                    socket_, writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::enableTunnelMode() { tunnelMode_ = true; }

void TCPConnection::relayClientToRemote() {
    if (config_->runMode() == RunMode::agent) {
        if (!socket_.is_open() || !client_->isOpen()) {
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

                            auto handler =
                                    [self](const boost::system::error_code &werr, std::size_t) {
                                        if (werr) {
                                            self->socketShutdown();
                                            return;
                                        }
                                        self->relayClientToRemote();
                                    };

                            if (self->client_->tlsEnabled()) {
                                boost::asio::async_write(
                                        self->client_->sslSocket(),
                                        boost::asio::buffer(self->downstreamBuf_.data(), bytes),
                                        boost::asio::bind_executor(self->strand_, handler));
                            } else {
                                boost::asio::async_write(
                                        self->client_->socket(),
                                        boost::asio::buffer(self->downstreamBuf_.data(), bytes),
                                        boost::asio::bind_executor(self->strand_, handler));
                            }
                        }));
        return;
    }

    if (!client_->isOpen()) {
        socketShutdown();
        return;
    }

    auto readHandler =
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
            };

    if (config_->server().tlsEnable) {
        if (!tlsSocket_) {
            socketShutdown();
            return;
        }

        tlsSocket_->async_read_some(
                boost::asio::buffer(downstreamBuf_),
                boost::asio::bind_executor(strand_, readHandler));
    } else {
        if (!socket_.is_open()) {
            socketShutdown();
            return;
        }

        socket_.async_read_some(
                boost::asio::buffer(downstreamBuf_),
                boost::asio::bind_executor(strand_, readHandler));
    }
}

void TCPConnection::relayRemoteToClient() {
    if (config_->runMode() == RunMode::agent) {
        if (!socket_.is_open() || !client_->isOpen()) {
            socketShutdown();
            return;
        }

        auto readHandler =
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
                };

        if (client_->tlsEnabled()) {
            client_->sslSocket().async_read_some(
                    boost::asio::buffer(upstreamBuf_),
                    boost::asio::bind_executor(strand_, readHandler));
        } else {
            client_->socket().async_read_some(
                    boost::asio::buffer(upstreamBuf_),
                    boost::asio::bind_executor(strand_, readHandler));
        }
        return;
    }

    if (!client_->isOpen()) {
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

                        auto writeHandler =
                                [self](const boost::system::error_code &werr, std::size_t) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }
                                    self->relayRemoteToClient();
                                };

                        if (self->config_->server().tlsEnable) {
                            if (!self->tlsSocket_) {
                                self->socketShutdown();
                                return;
                            }

                            boost::asio::async_write(
                                    *self->tlsSocket_,
                                    boost::asio::buffer(self->upstreamBuf_.data(), bytes),
                                    boost::asio::bind_executor(self->strand_, writeHandler));
                        } else {
                            if (!self->socket_.is_open()) {
                                self->socketShutdown();
                                return;
                            }

                            boost::asio::async_write(
                                    self->socket_,
                                    boost::asio::buffer(self->upstreamBuf_.data(), bytes),
                                    boost::asio::bind_executor(self->strand_, writeHandler));
                        }
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

        if (tlsSocket_) {
            tlsSocket_->shutdown(ignored);
            tlsSocket_->lowest_layer().shutdown(
                    boost::asio::ip::tcp::socket::shutdown_both, ignored);
            tlsSocket_->lowest_layer().close(ignored);
        }

        if (socket_.is_open()) {
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
            socket_.close(ignored);
        }

        if (client_) {
            client_->socketShutdown();
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection socketShutdown] [catch] " +
                            error.what(),
                    Log::Level::DEBUG);
    }
}