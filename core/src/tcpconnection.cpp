#include "tcpconnection.hpp"

#include <openssl/ssl.h>

#include <algorithm>
#include <array>
#include <boost/bind/bind.hpp>
#include <cctype>
#include <sstream>
#include <utility>

static std::string buildTunnelPostStream(const std::string &host,
                                         const std::string &userAgent) {
    std::ostringstream ss;

    ss << "POST / HTTP/1.1\r\n"
       << "Host: " << host << "\r\n"
       << "User-Agent: " << userAgent << "\r\n"
       << "Accept: */*\r\n"
       << "Content-Type: application/octet-stream\r\n"
       << "Transfer-Encoding: chunked\r\n"
       << "Connection: keep-alive\r\n"
       << "\r\n";

    return ss.str();
}

static std::string buildTunnelResponseStream() {
    std::ostringstream ss;

    ss << "HTTP/1.1 200 OK\r\n"
       << "Content-Type: application/octet-stream\r\n"
       << "Transfer-Encoding: chunked\r\n"
       << "Connection: keep-alive\r\n"
       << "\r\n";

    return ss.str();
}

static std::string makeEncryptedBody(const char *data,
                                     std::size_t bytes,
                                     const std::string &token) {
    const std::string plain(data, bytes);

    const BoolStr encrypted = aes256Encrypt(plain, token);

    if (encrypted.message == "FAILED") {
        return {};
    }

    return encode64(encrypted.message);
}

static std::string decryptHttpBody(const std::string &body,
                                   const std::string &token) {
    const BoolStr decrypted = aes256Decrypt(decode64(body), token);

    if (decrypted.message == "FAILED") {
        return {};
    }

    return decrypted.message;
}

static std::string buildEncryptedChunk(const char *data,
                                       std::size_t bytes,
                                       const std::string &token) {
    const std::string encryptedBody = makeEncryptedBody(data, bytes, token);

    if (encryptedBody.empty()) {
        return {};
    }

    std::ostringstream ss;

    ss << std::hex << encryptedBody.size() << "\r\n"
       << encryptedBody << "\r\n";

    return ss.str();
}

static bool consumeHttpHeaders(boost::asio::streambuf &buf,
                               std::string &headers) {
    const std::string packet = streambufToString(buf);
    const auto pos = packet.find("\r\n\r\n");

    if (pos == std::string::npos) {
        return false;
    }

    headers = packet.substr(0, pos + 4);
    buf.consume(pos + 4);

    return true;
}

template<typename Stream>
static bool readChunk(Stream &stream,
                      boost::asio::streambuf &buf,
                      std::string &chunkBody,
                      boost::system::error_code &ec) {
    boost::asio::read_until(stream, buf, "\r\n", ec);

    if (ec) {
        return false;
    }

    std::istream is(&buf);

    std::string sizeLine;
    std::getline(is, sizeLine);

    if (!sizeLine.empty() && sizeLine.back() == '\r') {
        sizeLine.pop_back();
    }

    const auto semicolon = sizeLine.find(';');

    if (semicolon != std::string::npos) {
        sizeLine = sizeLine.substr(0, semicolon);
    }

    std::size_t chunkSize = 0;

    try {
        chunkSize = std::stoul(sizeLine, nullptr, 16);
    } catch (...) {
        return false;
    }

    if (chunkSize == 0) {
        return false;
    }

    if (buf.size() < chunkSize + 2) {
        boost::asio::read(
                stream,
                buf,
                boost::asio::transfer_exactly(chunkSize + 2 - buf.size()),
                ec);

        if (ec) {
            return false;
        }
    }

    chunkBody.resize(chunkSize);
    is.read(chunkBody.data(), static_cast<std::streamsize>(chunkSize));

    char cr = 0;
    char lf = 0;

    is.get(cr);
    is.get(lf);

    return cr == '\r' && lf == '\n';
}

template<typename Stream>
static bool readRemainingHttpBody(Stream &stream,
                                  boost::asio::streambuf &buf,
                                  boost::system::error_code &ec) {
    std::string current = streambufToString(buf);
    const std::string headers = extractHeaders(current);
    const std::string body = extractBody(current);

    std::size_t contentLength = 0;

    if (parseContentLength(headers, contentLength)) {
        if (body.size() < contentLength) {
            boost::asio::read(
                    stream,
                    buf,
                    boost::asio::transfer_exactly(contentLength - body.size()),
                    ec);

            if (ec) {
                return false;
            }
        }

        return true;
    }

    if (isChunked(headers)) {
        while (true) {
            current = streambufToString(buf);

            const auto pos = current.find("\r\n\r\n");

            if (pos == std::string::npos) {
                return false;
            }

            const auto bodyPart = current.substr(pos + 4);

            if (bodyPart.find("\r\n0\r\n\r\n") != std::string::npos ||
                bodyPart == "0\r\n\r\n") {
                return true;
            }

            std::array<char, 4096> tmp{};

            const std::size_t n =
                    stream.read_some(boost::asio::buffer(tmp), ec);

            if (ec) {
                return false;
            }

            std::ostream os(&buf);
            os.write(tmp.data(), static_cast<std::streamsize>(n));
        }
    }

    return true;
}

TCPConnection::pointer TCPConnection::create(
        boost::asio::io_context &ioContext,
        const std::shared_ptr<Config> &config,
        const std::shared_ptr<Log> &log,
        TCPClient::pointer client) {
    return pointer(new TCPConnection(ioContext, config, log, std::move(client)));
}

TCPConnection::TCPConnection(boost::asio::io_context &ioContext,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             TCPClient::pointer client)
    : socket_(ioContext),
      sslContext_(boost::asio::ssl::context::tls_server),
      tlsSocket_(nullptr),
      config_(config),
      log_(log),
      ioContext_(ioContext),
      client_(std::move(client)),
      timeout_(ioContext),
      strand_(boost::asio::make_strand(ioContext)) {
    uuid_ = boost::uuids::random_generator()();
}

boost::asio::ip::tcp::socket &TCPConnection::socket() {
    return socket_;
}

TCPConnection::ssl_stream &TCPConnection::tlsSocket() {
    return *tlsSocket_;
}

void TCPConnection::writeBuffer(boost::asio::streambuf &buffer) {
    moveStreambuf(buffer, writeBuffer_);
}

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
        sslContext_.use_private_key_file(
                config_->server().tlsKeyFile,
                boost::asio::ssl::context::pem);

        if (!SSL_CTX_check_private_key(sslContext_.native_handle())) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection initTlsServerContext] "
                                "private key does not match certificate",
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
                                "] [TCPConnection initTlsServerContext] "
                                "failed to set cipher list",
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

        tlsSocket_ = std::make_unique<ssl_stream>(ioContext_, sslContext_);

        return true;
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection initTlsServerContext] " +
                            error.what(),
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
    } catch (const std::exception &error) {
        cancelTimeout();

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doHandshakeServer] " +
                            error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

void TCPConnection::startAgent() {
    std::lock_guard<std::mutex> lock(mutex_);

    client_->uuid_ = uuid_;

    doReadAgent();
}

void TCPConnection::startServer() {
    std::lock_guard<std::mutex> lock(mutex_);

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
                socket_,
                readBuffer_,
                boost::asio::transfer_at_least(1),
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadAgent,
                                    shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doReadAgent] [catch] " +
                            error.what(),
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
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doReadServer] [catch] " +
                            error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleReadAgent(const boost::system::error_code &error,
                                    std::size_t) {
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
                                    "] [TCPConnection handleReadAgent] " +
                                    error.message(),
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
                                "] [TCPConnection handleReadAgent] [errorIn] " +
                                errorIn.message(),
                        Log::Level::DEBUG);
            socketShutdown();
            return;
        }

        current = streambufToString(readBuffer_);

        const std::string headers = extractHeaders(current);
        const std::string body = extractBody(current);

        const bool isConnect =
                headers.rfind("CONNECT ", 0) == 0 ||
                headers.rfind("connect ", 0) == 0;

        if (!isConnect) {
            std::size_t contentLength = 0;

            if (parseContentLength(headers, contentLength) &&
                body.size() < contentLength) {
                boost::asio::read(
                        socket_,
                        readBuffer_,
                        boost::asio::transfer_exactly(contentLength - body.size()),
                        errorIn);

                if (errorIn && errorIn != boost::asio::error::eof) {
                    log_->write("[" + to_string(uuid_) +
                                        "] [TCPConnection handleReadAgent] "
                                        "[body read] " +
                                        errorIn.message(),
                                Log::Level::DEBUG);
                    socketShutdown();
                    return;
                }
            }
        }

        const std::string remotePeer =
                socket_.remote_endpoint().address().to_string() + ":" +
                std::to_string(socket_.remote_endpoint().port());

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] "
                            "[SRC " +
                            remotePeer + "] [Bytes " +
                            std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        AgentHandler::pointer agentHandler = AgentHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
                remotePeer,
                uuid_);

        agentHandler->handle();

        end_ = agentHandler->end_;
        connect_ = agentHandler->connect_;

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
                        [self = shared_from_this()](
                                const boost::system::error_code &ec,
                                std::size_t) {
                            self->cancelTimeout();

                            if (ec) {
                                self->log_->write(
                                        "[" + to_string(self->uuid_) +
                                                "] [TCPConnection write agent] " +
                                                ec.message(),
                                        Log::Level::ERROR);
                                self->socketShutdown();
                                return;
                            }

                            if (self->connect_) {
                                self->clearTunnelBuffers();
                                self->enableTunnelMode();
                                self->relayClientToRemote();
                                self->relayRemoteToClient();
                                return;
                            }

                            self->doReadAgent();
                        }));
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadAgent] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::handleReadServer(const boost::system::error_code &error,
                                     std::size_t) {
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

        boost::system::error_code bodyError;
        std::string remotePeer;

        if (config_->server().tlsEnable) {
            if (!readRemainingHttpBody(*tlsSocket_, readBuffer_, bodyError)) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] "
                                    "body read failed: " +
                                    bodyError.message(),
                            Log::Level::ERROR);
                socketShutdown();
                return;
            }

            remotePeer =
                    tlsSocket_->lowest_layer().remote_endpoint().address().to_string() +
                    ":" +
                    std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port());
        } else {
            if (!readRemainingHttpBody(socket_, readBuffer_, bodyError)) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TCPConnection handleReadServer] "
                                    "body read failed: " +
                                    bodyError.message(),
                            Log::Level::ERROR);
                socketShutdown();
                return;
            }

            remotePeer =
                    socket_.remote_endpoint().address().to_string() + ":" +
                    std::to_string(socket_.remote_endpoint().port());
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] "
                            "[SRC " +
                            remotePeer + "] [Bytes " +
                            std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        ServerHandler::pointer serverHandler = ServerHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
                remotePeer,
                uuid_);

        serverHandler->handle();

        end_ = serverHandler->end_;
        connect_ = serverHandler->connect_;

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
                        self->log_->write(
                                "[" + to_string(self->uuid_) +
                                        "] [TCPConnection write server] " +
                                        ec.message(),
                                Log::Level::ERROR);
                        self->socketShutdown();
                        return;
                    }

                    if (self->connect_) {
                        self->clearTunnelBuffers();
                        self->enableTunnelMode();
                        self->relayClientToRemote();
                        self->relayRemoteToClient();
                        return;
                    }

                    self->doReadServer();
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
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::enableTunnelMode() {
    tunnelMode_ = true;
}

void TCPConnection::clearTunnelBuffers() {
    readBuffer_.consume(readBuffer_.size());
    writeBuffer_.consume(writeBuffer_.size());
    downstreamHttpBuffer_.consume(downstreamHttpBuffer_.size());
    upstreamHttpBuffer_.consume(upstreamHttpBuffer_.size());

    tunnelRequestHeaderSent_ = false;
    tunnelResponseHeaderSent_ = false;
    tunnelRequestHeaderReceived_ = false;
    tunnelResponseHeaderReceived_ = false;
}

void TCPConnection::relayClientToRemote() {
    if (config_->runMode() == RunMode::agent) {
        if (!socket_.is_open() || !client_->isOpen()) {
            socketShutdown();
            return;
        }

        auto startRawRead = [self = shared_from_this()]() {
            self->socket_.async_read_some(
                    boost::asio::buffer(self->downstreamBuf_),
                    boost::asio::bind_executor(
                            self->strand_,
                            [self](const boost::system::error_code &ec,
                                   std::size_t bytes) {
                                if (ec || bytes == 0) {
                                    self->socketShutdown();
                                    return;
                                }

                                auto chunk = std::make_shared<std::string>(
                                        buildEncryptedChunk(
                                                self->downstreamBuf_.data(),
                                                bytes,
                                                self->config_->general().token));

                                if (chunk->empty()) {
                                    self->socketShutdown();
                                    return;
                                }

                                auto writeHandler =
                                        [self, chunk](
                                                const boost::system::error_code &werr,
                                                std::size_t) {
                                            if (werr) {
                                                self->socketShutdown();
                                                return;
                                            }

                                            self->relayClientToRemote();
                                        };

                                if (self->client_->tlsEnabled()) {
                                    boost::asio::async_write(
                                            self->client_->sslSocket(),
                                            boost::asio::buffer(*chunk),
                                            boost::asio::bind_executor(
                                                    self->strand_,
                                                    writeHandler));
                                } else {
                                    boost::asio::async_write(
                                            self->client_->socket(),
                                            boost::asio::buffer(*chunk),
                                            boost::asio::bind_executor(
                                                    self->strand_,
                                                    writeHandler));
                                }
                            }));
        };

        if (!tunnelRequestHeaderSent_) {
            tunnelRequestHeaderSent_ = true;

            auto header = std::make_shared<std::string>(
                    buildTunnelPostStream("relay", config_->agent().userAgent));

            auto headerHandler =
                    [self = shared_from_this(), header, startRawRead](
                            const boost::system::error_code &ec,
                            std::size_t) {
                        if (ec) {
                            self->socketShutdown();
                            return;
                        }

                        startRawRead();
                    };

            if (client_->tlsEnabled()) {
                boost::asio::async_write(
                        client_->sslSocket(),
                        boost::asio::buffer(*header),
                        boost::asio::bind_executor(strand_, headerHandler));
            } else {
                boost::asio::async_write(
                        client_->socket(),
                        boost::asio::buffer(*header),
                        boost::asio::bind_executor(strand_, headerHandler));
            }

            return;
        }

        startRawRead();
        return;
    }

    if (!client_->isOpen()) {
        socketShutdown();
        return;
    }

    if (!tunnelRequestHeaderReceived_) {
        auto headerHandler =
                [self = shared_from_this()](const boost::system::error_code &ec,
                                            std::size_t) {
                    if (ec) {
                        self->socketShutdown();
                        return;
                    }

                    std::string headers;

                    if (!consumeHttpHeaders(self->downstreamHttpBuffer_, headers)) {
                        self->socketShutdown();
                        return;
                    }

                    if (headers.rfind("POST / ", 0) != 0) {
                        self->log_->write(
                                "[" + to_string(self->uuid_) +
                                        "] [relayClientToRemote server] "
                                        "expected POST / stream header, got: " +
                                        headers.substr(
                                                0,
                                                std::min<std::size_t>(
                                                        headers.size(),
                                                        120)),
                                Log::Level::ERROR);
                        self->socketShutdown();
                        return;
                    }

                    self->tunnelRequestHeaderReceived_ = true;
                    self->relayClientToRemote();
                };

        if (config_->server().tlsEnable) {
            if (!tlsSocket_) {
                socketShutdown();
                return;
            }

            boost::asio::async_read_until(
                    *tlsSocket_,
                    downstreamHttpBuffer_,
                    "\r\n\r\n",
                    boost::asio::bind_executor(strand_, headerHandler));
        } else {
            boost::asio::async_read_until(
                    socket_,
                    downstreamHttpBuffer_,
                    "\r\n\r\n",
                    boost::asio::bind_executor(strand_, headerHandler));
        }

        return;
    }

    auto readHandler =
            [self = shared_from_this()](const boost::system::error_code &ec,
                                        std::size_t) {
                if (ec) {
                    self->socketShutdown();
                    return;
                }

                std::string chunkBody;
                boost::system::error_code chunkError;

                if (self->config_->server().tlsEnable) {
                    if (!self->tlsSocket_ ||
                        !readChunk(*self->tlsSocket_,
                                   self->downstreamHttpBuffer_,
                                   chunkBody,
                                   chunkError)) {
                        self->socketShutdown();
                        return;
                    }
                } else {
                    if (!readChunk(self->socket_,
                                   self->downstreamHttpBuffer_,
                                   chunkBody,
                                   chunkError)) {
                        self->socketShutdown();
                        return;
                    }
                }

                std::string plain;

                try {
                    plain = decryptHttpBody(
                            chunkBody,
                            self->config_->general().token);
                } catch (const std::exception &error) {
                    self->log_->write(
                            "[" + to_string(self->uuid_) +
                                    "] [relayClientToRemote server] "
                                    "decrypt failed: " +
                                    error.what(),
                            Log::Level::ERROR);
                    self->socketShutdown();
                    return;
                }

                if (plain.empty()) {
                    self->socketShutdown();
                    return;
                }

                auto rawData = std::make_shared<std::string>(std::move(plain));

                boost::asio::async_write(
                        self->client_->socket(),
                        boost::asio::buffer(*rawData),
                        boost::asio::bind_executor(
                                self->strand_,
                                [self, rawData](
                                        const boost::system::error_code &werr,
                                        std::size_t) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayClientToRemote();
                                }));
            };

    if (config_->server().tlsEnable) {
        boost::asio::async_read_until(
                *tlsSocket_,
                downstreamHttpBuffer_,
                "\r\n",
                boost::asio::bind_executor(strand_, readHandler));
    } else {
        boost::asio::async_read_until(
                socket_,
                downstreamHttpBuffer_,
                "\r\n",
                boost::asio::bind_executor(strand_, readHandler));
    }
}

void TCPConnection::relayRemoteToClient() {
    if (config_->runMode() == RunMode::agent) {
        if (!socket_.is_open() || !client_->isOpen()) {
            socketShutdown();
            return;
        }

        if (!tunnelResponseHeaderReceived_) {
            auto headerHandler =
                    [self = shared_from_this()](
                            const boost::system::error_code &ec,
                            std::size_t) {
                        if (ec) {
                            self->socketShutdown();
                            return;
                        }

                        std::string headers;

                        if (!consumeHttpHeaders(self->upstreamHttpBuffer_, headers)) {
                            self->socketShutdown();
                            return;
                        }

                        if (headers.rfind("HTTP/1.1 200", 0) != 0) {
                            self->log_->write(
                                    "[" + to_string(self->uuid_) +
                                            "] [relayRemoteToClient agent] "
                                            "expected HTTP 200 stream header, got: " +
                                            headers.substr(
                                                    0,
                                                    std::min<std::size_t>(
                                                            headers.size(),
                                                            120)),
                                    Log::Level::ERROR);
                            self->socketShutdown();
                            return;
                        }

                        self->tunnelResponseHeaderReceived_ = true;
                        self->relayRemoteToClient();
                    };

            if (client_->tlsEnabled()) {
                boost::asio::async_read_until(
                        client_->sslSocket(),
                        upstreamHttpBuffer_,
                        "\r\n\r\n",
                        boost::asio::bind_executor(strand_, headerHandler));
            } else {
                boost::asio::async_read_until(
                        client_->socket(),
                        upstreamHttpBuffer_,
                        "\r\n\r\n",
                        boost::asio::bind_executor(strand_, headerHandler));
            }

            return;
        }

        auto readHandler =
                [self = shared_from_this()](
                        const boost::system::error_code &ec,
                        std::size_t) {
                    if (ec) {
                        self->socketShutdown();
                        return;
                    }

                    std::string chunkBody;
                    boost::system::error_code chunkError;

                    if (self->client_->tlsEnabled()) {
                        if (!readChunk(self->client_->sslSocket(),
                                       self->upstreamHttpBuffer_,
                                       chunkBody,
                                       chunkError)) {
                            self->socketShutdown();
                            return;
                        }
                    } else {
                        if (!readChunk(self->client_->socket(),
                                       self->upstreamHttpBuffer_,
                                       chunkBody,
                                       chunkError)) {
                            self->socketShutdown();
                            return;
                        }
                    }

                    std::string plain;

                    try {
                        plain = decryptHttpBody(
                                chunkBody,
                                self->config_->general().token);
                    } catch (const std::exception &error) {
                        self->log_->write(
                                "[" + to_string(self->uuid_) +
                                        "] [relayRemoteToClient agent] "
                                        "decrypt failed: " +
                                        error.what(),
                                Log::Level::ERROR);
                        self->socketShutdown();
                        return;
                    }

                    if (plain.empty()) {
                        self->socketShutdown();
                        return;
                    }

                    auto rawData = std::make_shared<std::string>(std::move(plain));

                    boost::asio::async_write(
                            self->socket_,
                            boost::asio::buffer(*rawData),
                            boost::asio::bind_executor(
                                    self->strand_,
                                    [self, rawData](
                                            const boost::system::error_code &werr,
                                            std::size_t) {
                                        if (werr) {
                                            self->socketShutdown();
                                            return;
                                        }

                                        self->relayRemoteToClient();
                                    }));
                };

        if (client_->tlsEnabled()) {
            boost::asio::async_read_until(
                    client_->sslSocket(),
                    upstreamHttpBuffer_,
                    "\r\n",
                    boost::asio::bind_executor(strand_, readHandler));
        } else {
            boost::asio::async_read_until(
                    client_->socket(),
                    upstreamHttpBuffer_,
                    "\r\n",
                    boost::asio::bind_executor(strand_, readHandler));
        }

        return;
    }

    if (!client_->isOpen()) {
        socketShutdown();
        return;
    }

    auto startRawRead = [self = shared_from_this()]() {
        self->client_->socket().async_read_some(
                boost::asio::buffer(self->upstreamBuf_),
                boost::asio::bind_executor(
                        self->strand_,
                        [self](const boost::system::error_code &ec,
                               std::size_t bytes) {
                            if (ec || bytes == 0) {
                                self->socketShutdown();
                                return;
                            }

                            auto chunk = std::make_shared<std::string>(
                                    buildEncryptedChunk(
                                            self->upstreamBuf_.data(),
                                            bytes,
                                            self->config_->general().token));

                            if (chunk->empty()) {
                                self->socketShutdown();
                                return;
                            }

                            auto writeHandler =
                                    [self, chunk](
                                            const boost::system::error_code &werr,
                                            std::size_t) {
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
                                        boost::asio::buffer(*chunk),
                                        boost::asio::bind_executor(
                                                self->strand_,
                                                writeHandler));
                            } else {
                                boost::asio::async_write(
                                        self->socket_,
                                        boost::asio::buffer(*chunk),
                                        boost::asio::bind_executor(
                                                self->strand_,
                                                writeHandler));
                            }
                        }));
    };

    if (!tunnelResponseHeaderSent_) {
        tunnelResponseHeaderSent_ = true;

        auto header = std::make_shared<std::string>(buildTunnelResponseStream());

        auto headerHandler =
                [self = shared_from_this(), header, startRawRead](
                        const boost::system::error_code &ec,
                        std::size_t) {
                    if (ec) {
                        self->socketShutdown();
                        return;
                    }

                    startRawRead();
                };

        if (config_->server().tlsEnable) {
            if (!tlsSocket_) {
                socketShutdown();
                return;
            }

            boost::asio::async_write(
                    *tlsSocket_,
                    boost::asio::buffer(*header),
                    boost::asio::bind_executor(strand_, headerHandler));
        } else {
            boost::asio::async_write(
                    socket_,
                    boost::asio::buffer(*header),
                    boost::asio::bind_executor(strand_, headerHandler));
        }

        return;
    }

    startRawRead();
}

void TCPConnection::resetTimeout() {
    if (!config_->general().timeout) {
        return;
    }

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));

    timeout_.async_wait(
            boost::asio::bind_executor(
                    strand_,
                    boost::bind(&TCPConnection::onTimeout,
                                shared_from_this(),
                                boost::asio::placeholders::error)));
}

void TCPConnection::cancelTimeout() {
    if (config_->general().timeout) {
        timeout_.cancel();
    }
}

void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) {
        return;
    }

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
                    boost::asio::ip::tcp::socket::shutdown_both,
                    ignored);
            tlsSocket_->lowest_layer().close(ignored);
        }

        if (socket_.is_open()) {
            socket_.shutdown(
                    boost::asio::ip::tcp::socket::shutdown_both,
                    ignored);
            socket_.close(ignored);
        }

        if (client_) {
            client_->socketShutdown();
        }
    } catch (const std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection socketShutdown] [catch] " +
                            error.what(),
                    Log::Level::DEBUG);
    }
}