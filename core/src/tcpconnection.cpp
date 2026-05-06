#include "tcpconnection.hpp"

#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

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
      pollTimer_(io_context),
      strand_(boost::asio::make_strand(io_context_)) {
    uuid_ = boost::uuids::random_generator()();
    end_ = false;
    connect_ = false;
    tunnelClosed_ = false;
    pollInProgress_ = false;
}

boost::asio::ip::tcp::socket &TCPConnection::socket() { return socket_; }

TCPConnection::ssl_stream &TCPConnection::tlsSocket() { return *tlsSocket_; }

bool TCPConnection::initTlsServerContext() {
    try {
        sslContext_ = boost::asio::ssl::context(boost::asio::ssl::context::tls_server);

        sslContext_.set_options(boost::asio::ssl::context::default_workarounds |
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

        if (SSL_CTX_set_cipher_list(sslContext_.native_handle(),
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
        SSL_CTX_set_ciphersuites(sslContext_.native_handle(),
                                 "TLS_AES_256_GCM_SHA384:"
                                 "TLS_AES_128_GCM_SHA256:"
                                 "TLS_CHACHA20_POLY1305_SHA256");
#endif

        tlsSocket_ = std::make_unique<ssl_stream>(io_context_, sslContext_);
        return true;
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection initTlsServerContext] " + error.what(),
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
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doHandshakeServer] " + error.what(),
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
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();

        boost::asio::async_read(socket_,
                                readBuffer_,
                                boost::asio::transfer_at_least(1),
                                boost::asio::bind_executor(
                                        strand_,
                                        boost::bind(&TCPConnection::handleReadAgent,
                                                    shared_from_this(),
                                                    boost::asio::placeholders::error,
                                                    boost::asio::placeholders::bytes_transferred)));
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadAgent] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::doReadServer() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();

        auto handler = boost::asio::bind_executor(
                strand_,
                boost::bind(&TCPConnection::handleReadServer,
                            shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));

        if (config_->server().tlsEnable) {
            boost::asio::async_read_until(*tlsSocket_, readBuffer_, "\r\n\r\n", handler);
        } else {
            boost::asio::async_read_until(socket_, readBuffer_, "\r\n\r\n", handler);
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadServer] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleReadAgent(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();

        if (error) {
            socketShutdown();
            return;
        }

        boost::system::error_code errorIn;

        std::string current = streambufToString(readBuffer_);
        if (current.find("\r\n\r\n") == std::string::npos) {
            boost::asio::read_until(socket_, readBuffer_, "\r\n\r\n", errorIn);
        }

        if (errorIn && errorIn != boost::asio::error::eof) {
            socketShutdown();
            return;
        }

        current = streambufToString(readBuffer_);

        const std::string headers = HttpUtils::extractHeaders(current);
        const std::string body = HttpUtils::extractBody(current);

        const bool isConnect =
                headers.rfind("CONNECT ", 0) == 0 ||
                headers.rfind("connect ", 0) == 0;

        if (!isConnect) {
            std::size_t contentLength = 0;

            if (HttpUtils::parseContentLength(headers, contentLength) &&
                body.size() < contentLength) {
                boost::asio::read(socket_,
                                  readBuffer_,
                                  boost::asio::transfer_exactly(contentLength - body.size()),
                                  errorIn);

                if (errorIn && errorIn != boost::asio::error::eof) {
                    socketShutdown();
                    return;
                }
            }
        }

        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [SRC " +
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
                        [self = shared_from_this()](const boost::system::error_code &ec, std::size_t) {
                            self->cancelTimeout();

                            if (ec) {
                                self->socketShutdown();
                                return;
                            }

                            if (self->connect_) {
                                self->tunnelClosed_ = false;

                                if (self->config_->general().tunnelEnable) {
                                    self->enableDirectTunnelMode();
                                    self->relayClientToServer();
                                    self->relayServerToClient();
                                } else {
                                    self->client_->socketShutdown();
                                    self->startTunnelReadFromClient();
                                    self->startTunnelPollServer();
                                }

                                return;
                            }
                            self->doReadAgent();
                        }));
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [catch read] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::handleReadServer(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();

        if (error) {
            socketShutdown();
            return;
        }

        boost::system::error_code bodyError;
        std::string remotePeer;

        if (config_->server().tlsEnable) {
            if (!HttpUtils::readRemainingHttpBody(*tlsSocket_, readBuffer_, bodyError)) {
                socketShutdown();
                return;
            }

            remotePeer =
                    tlsSocket_->lowest_layer().remote_endpoint().address().to_string() +
                    ":" +
                    std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port());
        } else {
            if (!HttpUtils::readRemainingHttpBody(socket_, readBuffer_, bodyError)) {
                socketShutdown();
                return;
            }

            remotePeer =
                    socket_.remote_endpoint().address().to_string() +
                    ":" +
                    std::to_string(socket_.remote_endpoint().port());
        }

        log_->write(
                "[" + to_string(uuid_) + "] [TCPConnection handleReadServer] [SRC " +
                        remotePeer +
                        "] [Bytes " +
                        std::to_string(readBuffer_.size()) +
                        "] ",
                Log::Level::DEBUG);

        ServerHandler::pointer serverHandler_ = ServerHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
                remotePeer,
                uuid_);

        serverHandler_->handle();

        end_ = serverHandler_->end_;
        connect_ = serverHandler_->connect_;

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();

        auto writeHandler =
                [self = shared_from_this()](
                        const boost::system::error_code &ec,
                        std::size_t) {
                    self->cancelTimeout();

                    if (ec) {
                        self->socketShutdown();
                        return;
                    }

                    if (self->connect_ && self->config_->general().tunnelEnable) {
                        self->enableDirectTunnelMode();
                        self->relayAgentToTarget();
                        self->relayTargetToAgent();
                        return;
                    }

                    self->socketShutdown();
                };

        if (config_->server().tlsEnable) {
            boost::asio::async_write(
                    *tlsSocket_,
                    writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        } else {
            boost::asio::async_write(
                    socket_,
                    writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        }
    } catch (std::exception &error) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [TCPConnection handleReadServer] [catch read] " +
                        error.what(),
                Log::Level::DEBUG);

        socketShutdown();
    }
}

std::string TCPConnection::makeRelayHostHeader() const {
    std::string hostHeader = config_->general().fakeUrl;

    auto pos = hostHeader.find("://");
    if (pos != std::string::npos) hostHeader = hostHeader.substr(pos + 3);

    pos = hostHeader.find('/');
    if (pos != std::string::npos) hostHeader = hostHeader.substr(0, pos);

    return hostHeader;
}

std::string TCPConnection::postTunnelAction(const std::string &action, const std::string &rawBody) {
    TCPClient::pointer c = TCPClient::create(io_context_, config_, log_);
    c->uuid_ = uuid_;

    if (config_->agent().tlsEnable && !c->enableTlsClient()) return "";
    if (!c->doConnect(config_->agent().serverIp, config_->agent().serverPort)) return "";
    if (c->tlsEnabled() && !c->doHandshakeClient()) return "";

    BoolStr enc = aes256Encrypt(
            hexArrToStr(reinterpret_cast<const unsigned char *>(rawBody.data()), rawBody.size()),
            config_->general().token);

    if (!enc.ok) return "";

    const std::string encodedBody = encode64(enc.message);

    std::ostringstream req;
    req << config_->randomMethod() + " /" + config_->randomEndPoint() + " HTTP/1.1\r\n"
        << "Host: " << makeRelayHostHeader() << "\r\n"
        << "User-Agent: Mozilla/5.0\r\n"
        << "Accept: */*\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "X-Nipo-Session: " << to_string(uuid_) << "\r\n"
        << "X-Nipo-Action: " << action << "\r\n"
        << "Content-Length: " << encodedBody.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << encodedBody;

    boost::asio::streambuf out;
    copyStringToStreambuf(req.str(), out);

    c->doWrite(out);
    c->doReadAgent();

    const std::string response = streambufToString(c->readBuffer());

    c->socketShutdown();

    const auto pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return "";

    const std::string encryptedBody = response.substr(pos + 4);
    if (encryptedBody.empty()) return "";

    BoolStr dec = aes256Decrypt(encryptedBody, config_->general().token);
    if (!dec.ok) return "";

    return dec.message;
}

void TCPConnection::startTunnelReadFromClient() {
    if (tunnelClosed_ || !socket_.is_open()) return;

    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->closeTunnelSession();
                            self->socketShutdown();
                            return;
                        }

                        std::string raw(self->downstreamBuf_.data(), bytes);

                        self->postTunnelAction("send", raw);
                        self->startTunnelReadFromClient();
                    }));
}

void TCPConnection::startTunnelPollServer() {
    if (tunnelClosed_ || !socket_.is_open() || pollInProgress_) return;

    pollInProgress_ = true;

    pollTimer_.expires_after(std::chrono::milliseconds(10));
    pollTimer_.async_wait(
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec) {
                        self->pollInProgress_ = false;

                        if (ec || self->tunnelClosed_ || !self->socket_.is_open()) return;

                        std::string data = self->postTunnelAction("recv", "");

                        if (!data.empty()) {
                            boost::asio::async_write(
                                    self->socket_,
                                    boost::asio::buffer(data),
                                    boost::asio::bind_executor(
                                            self->strand_,
                                            [self](const boost::system::error_code &werr, std::size_t) {
                                                if (werr) {
                                                    self->closeTunnelSession();
                                                    self->socketShutdown();
                                                    return;
                                                }

                                                self->startTunnelPollServer();
                                            }));
                            return;
                        }

                        self->startTunnelPollServer();
                    }));
}

void TCPConnection::closeTunnelSession() {
    if (tunnelClosed_) return;

    tunnelClosed_ = true;
    pollTimer_.cancel();

    postTunnelAction("close", "");
}

void TCPConnection::resetTimeout() {
    if (!config_->general().timeout) return;

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPConnection::onTimeout,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
}

void TCPConnection::cancelTimeout() {
    if (config_->general().timeout) timeout_.cancel();
}

void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;
    if (connect_) return;

    log_->write("[" + to_string(uuid_) + "] [TCPConnection onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);

    socketShutdown();
}

void TCPConnection::socketShutdown() {
    try {
        boost::system::error_code ignored;

        pollTimer_.cancel();
        timeout_.cancel();

        if (tlsSocket_) {
            tlsSocket_->shutdown(ignored);
            tlsSocket_->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
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
        log_->write("[" + to_string(uuid_) + "] [TCPConnection socketShutdown] [catch] " + error.what(),
                    Log::Level::DEBUG);
    }
}

void TCPConnection::enableDirectTunnelMode() {
    tunnelMode_ = true;
}

void TCPConnection::asyncWriteToAgentConnection(
        const char *data,
        std::size_t bytes,
        std::function<void(const boost::system::error_code &)> done) {
    if (config_->server().tlsEnable && tlsSocket_) {
        boost::asio::async_write(
                *tlsSocket_,
                boost::asio::buffer(data, bytes),
                boost::asio::bind_executor(
                        strand_,
                        [done](const boost::system::error_code &ec, std::size_t) {
                            done(ec);
                        }));
        return;
    }

    boost::asio::async_write(
            socket_,
            boost::asio::buffer(data, bytes),
            boost::asio::bind_executor(
                    strand_,
                    [done](const boost::system::error_code &ec, std::size_t) {
                        done(ec);
                    }));
}

void TCPConnection::asyncReadFromAgentConnection(
        std::array<char, 8192> &buffer,
        std::function<void(const boost::system::error_code &, std::size_t)> done) {
    if (config_->server().tlsEnable && tlsSocket_) {
        tlsSocket_->async_read_some(
                boost::asio::buffer(buffer),
                boost::asio::bind_executor(strand_, done));
        return;
    }

    socket_.async_read_some(
            boost::asio::buffer(buffer),
            boost::asio::bind_executor(strand_, done));
}

void TCPConnection::asyncWriteToAgentServerConnection(
        const char *data,
        std::size_t bytes,
        std::function<void(const boost::system::error_code &)> done) {
    if (client_->tlsEnabled()) {
        boost::asio::async_write(
                client_->sslSocket(),
                boost::asio::buffer(data, bytes),
                boost::asio::bind_executor(
                        strand_,
                        [done](const boost::system::error_code &ec, std::size_t) {
                            done(ec);
                        }));
        return;
    }

    boost::asio::async_write(
            client_->socket(),
            boost::asio::buffer(data, bytes),
            boost::asio::bind_executor(
                    strand_,
                    [done](const boost::system::error_code &ec, std::size_t) {
                        done(ec);
                    }));
}

void TCPConnection::asyncReadFromAgentServerConnection(
        std::array<char, 8192> &buffer,
        std::function<void(const boost::system::error_code &, std::size_t)> done) {
    if (client_->tlsEnabled()) {
        client_->sslSocket().async_read_some(
                boost::asio::buffer(buffer),
                boost::asio::bind_executor(strand_, done));
        return;
    }

    client_->socket().async_read_some(
            boost::asio::buffer(buffer),
            boost::asio::bind_executor(strand_, done));
}

void TCPConnection::relayClientToServer() {
    if (tunnelClosed_ || !socket_.is_open() || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](
                            const boost::system::error_code &ec,
                            std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->socketShutdown();
                            return;
                        }

                        self->asyncWriteToAgentServerConnection(
                                self->downstreamBuf_.data(),
                                bytes,
                                [self](const boost::system::error_code &werr) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayClientToServer();
                                });
                    }));
}

void TCPConnection::relayServerToClient() {
    if (tunnelClosed_ || !socket_.is_open() || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    asyncReadFromAgentServerConnection(
            upstreamBuf_,
            [self = shared_from_this()](
                    const boost::system::error_code &ec,
                    std::size_t bytes) {
                if (ec || bytes == 0) {
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

                                    self->relayServerToClient();
                                }));
            });
}

void TCPConnection::relayAgentToTarget() {
    if (tunnelClosed_ || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    asyncReadFromAgentConnection(
            downstreamBuf_,
            [self = shared_from_this()](
                    const boost::system::error_code &ec,
                    std::size_t bytes) {
                if (ec || bytes == 0) {
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

                                    self->relayAgentToTarget();
                                }));
            });
}

void TCPConnection::relayTargetToAgent() {
    if (tunnelClosed_ || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    client_->socket().async_read_some(
            boost::asio::buffer(upstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](
                            const boost::system::error_code &ec,
                            std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->socketShutdown();
                            return;
                        }

                        self->asyncWriteToAgentConnection(
                                self->upstreamBuf_.data(),
                                bytes,
                                [self](const boost::system::error_code &werr) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayTargetToAgent();
                                });
                    }));
}

std::string TCPConnection::HttpUtils::toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string TCPConnection::HttpUtils::extractHeaders(const std::string &msg) {
    auto pos = msg.find("\r\n\r\n");
    if (pos == std::string::npos) return {};
    return msg.substr(0, pos + 4);
}

std::string TCPConnection::HttpUtils::extractBody(const std::string &msg) {
    auto pos = msg.find("\r\n\r\n");
    if (pos == std::string::npos) return {};
    return msg.substr(pos + 4);
}

bool TCPConnection::HttpUtils::parseContentLength(const std::string &headers, std::size_t &value) {
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

bool TCPConnection::HttpUtils::isChunked(const std::string &headers) {
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

bool TCPConnection::HttpUtils::readRemainingHttpBody(
        boost::asio::ip::tcp::socket &stream,
        boost::asio::streambuf &buf,
        boost::system::error_code &ec) {
    return readRemainingHttpBodyImpl(
            buf,
            ec,
            [&stream](boost::asio::mutable_buffer buffer, boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            });
}

bool TCPConnection::HttpUtils::readRemainingHttpBody(
        ssl_stream &stream,
        boost::asio::streambuf &buf,
        boost::system::error_code &ec) {
    return readRemainingHttpBodyImpl(
            buf,
            ec,
            [&stream](boost::asio::mutable_buffer buffer, boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            });
}

bool TCPConnection::HttpUtils::readRemainingHttpBodyImpl(
        boost::asio::streambuf &buf,
        boost::system::error_code &ec,
        const std::function<std::size_t(boost::asio::mutable_buffer,
                                        boost::system::error_code &)> &readSome) {
    std::string current = streambufToString(buf);
    std::string headers = extractHeaders(current);
    std::string body = extractBody(current);

    std::size_t contentLength = 0;

    if (parseContentLength(headers, contentLength)) {
        if (body.size() < contentLength) {
            std::vector<char> tmp(contentLength - body.size());
            std::size_t total = 0;

            while (total < tmp.size()) {
                std::size_t n = readSome(
                        boost::asio::buffer(tmp.data() + total, tmp.size() - total),
                        ec);

                if (ec) return false;
                total += n;
            }

            std::ostream os(&buf);
            os.write(tmp.data(), static_cast<std::streamsize>(tmp.size()));
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
            std::size_t n = readSome(boost::asio::buffer(tmp), ec);

            if (ec) return false;

            std::ostream os(&buf);
            os.write(tmp.data(), static_cast<std::streamsize>(n));
        }
    }

    return true;
}