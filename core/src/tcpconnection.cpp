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

    bool readRemainingHttpBody(TCPConnection::ssl_stream &stream,
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


/**
 * @brief Constructs a `TCPConnection` instance with I/O context, configuration,
 *        logging, and an associated outbound client.
 *
 * @details
 * - Initializes the accepted inbound socket.
 * - Initializes the TLS server context in server mode.
 * - Stores references to the configuration, logging object, and I/O context.
 * - Stores the associated outbound `TCPClient`.
 * - Initializes the timeout timer and serialized execution strand.
 * - Generates a unique UUID for the connection.
 * - Initializes internal state flags:
 *   - `end_` is set to `false`
 *   - `connect_` is set to `false`
 *   - `tunnelMode_` is set to `false`
 *
 * @param io_context Reference to the Boost.Asio I/O context.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 * @param client Shared pointer to the associated outbound `TCPClient`.
 */
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

/**
 * @brief Returns the inbound TCP socket associated with this connection.
 *
 * @return Reference to the inbound socket.
 */
boost::asio::ip::tcp::socket &TCPConnection::socket() { return socket_; }

/**
 * @brief Returns the TLS server stream associated with this connection.
 *
 * @return Reference to the TLS socket stream.
 */
TCPConnection::ssl_stream &TCPConnection::tlsSocket() { return *tlsSocket_; }

/**
 * @brief Initializes the TLS server context for inbound encrypted connections.
 *
 * @details
 * - Reinitializes the SSL context in TLS server mode.
 * - Applies secure context options and disables legacy SSL versions.
 * - Enforces a minimum TLS version of 1.2.
 * - Loads the configured server certificate and private key.
 * - Verifies that the private key matches the loaded certificate.
 * - Disables peer certificate verification.
 * - Sets the allowed TLS cipher list and TLS 1.3 cipher suites.
 * - Creates the TLS stream object bound to the internal I/O context.
 *
 * @return `true` if initialization succeeds, otherwise `false`.
 */
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

/**
 * @brief Performs the server-side TLS handshake on the inbound connection.
 *
 * @details
 * - Starts the timeout timer before the handshake.
 * - Executes the TLS handshake in server mode.
 * - Cancels the timeout timer on completion.
 * - Logs handshake errors when they occur.
 *
 * @return `true` if the handshake succeeds, otherwise `false`.
 */
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

/**
 * @brief Starts processing this connection in agent mode.
 *
 * @details
 * - Assigns the connection UUID to the associated outbound client.
 * - Begins asynchronous agent-side reading.
 */
void TCPConnection::startAgent() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadAgent();
}

/**
 * @brief Starts processing this connection in server mode.
 *
 * @details
 * - Assigns the connection UUID to the associated outbound client.
 * - Begins asynchronous server-side reading.
 */
void TCPConnection::startServer() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadServer();
}

/**
 * @brief Initiates asynchronous reading for agent-mode inbound traffic.
 *
 * @details
 * - If tunnel mode is enabled, switches directly to transparent relay mode.
 * - Clears the read and write buffers before starting a new operation.
 * - Starts the timeout timer.
 * - Performs an asynchronous read of at least one byte from the inbound socket.
 * - Dispatches completion to `handleReadAgent()` through the strand.
 */
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
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doReadAgent] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

/**
 * @brief Initiates asynchronous reading for server-mode inbound traffic.
 *
 * @details
 * - If tunnel mode is enabled, switches directly to transparent relay mode.
 * - Clears the read and write buffers before starting a new operation.
 * - Starts the timeout timer.
 * - Performs an asynchronous read until the application terminator
 *   `"COMP\r\n\r\n"` is encountered.
 * - Dispatches completion to `handleReadServer()` through the strand.
 */
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
                *tlsSocket_, readBuffer_, "\r\n\r\n",
                boost::asio::bind_executor(
                        strand_,
                        boost::bind(&TCPConnection::handleReadServer, shared_from_this(),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection doReadServer] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

/**
 * @brief Handles completion of an agent-side read operation.
 *
 * @details
 * - Cancels the timeout timer.
 * - Handles EOF and other socket errors by shutting down the connection.
 * - Reads the remainder of the request based on the first byte:
 *   - If the first byte is `C` (`43` in hex), reads until `"\r\n"`.
 *   - Otherwise reads until `"\r\n\r\n"`.
 * - Logs source endpoint information and the received byte count.
 * - Creates an `AgentHandler` to process the received request.
 * - Updates connection state based on the handler result.
 * - Asynchronously writes the generated response back to the inbound socket.
 * - Switches to tunnel relay mode when CONNECT handling is activated.
 */
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

        // We already have at least 1 byte from doReadAgent().
        // For HTTP proxy traffic, always read the full request headers first.
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

        // CONNECT requests do not have an HTTP message body here.
        const bool isConnect =
                headers.rfind("CONNECT ", 0) == 0 || headers.rfind("connect ", 0) == 0;

        // For normal HTTP requests, if Content-Length exists, make sure the full body is present.
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

/**
 * @brief Handles completion of a server-side read operation.
 *
 * @details
 * - Cancels the timeout timer.
 * - Handles EOF and other socket errors by shutting down the connection.
 * - Logs source endpoint information and the received byte count.
 * - Creates a `ServerHandler` to process the received request.
 * - Updates connection state based on the handler result.
 * - Asynchronously writes the generated response back through the TLS socket.
 * - Switches to tunnel relay mode when CONNECT handling is activated.
 */
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
        if (!readRemainingHttpBody(*tlsSocket_, readBuffer_, bodyError)) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection handleReadServer] body read failed: " +
                                bodyError.message(),
                        Log::Level::ERROR);
            socketShutdown();
            return;
        }

        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] "
                            "[SRC " +
                            tlsSocket_->lowest_layer().remote_endpoint().address().to_string() +
                            ":" +
                            std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port()) +
                            "] [Bytes " + std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        ServerHandler::pointer serverHandler_ = ServerHandler::create(
                readBuffer_, writeBuffer_, config_, log_, client_,
                tlsSocket_->lowest_layer().remote_endpoint().address().to_string() + ":" +
                        std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port()),
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
                *tlsSocket_, writeBuffer_.data(),
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
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection handleReadServer] [catch read] " +
                            error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

/**
 * @brief Enables transparent tunnel relay mode for this connection.
 *
 * @details
 * - After this is enabled, traffic is relayed bidirectionally without
 *   request/response parsing.
 */
void TCPConnection::enableTunnelMode() { tunnelMode_ = true; }

/**
 * @brief Relays traffic from the inbound client side to the remote side.
 *
 * @details
 * - In agent mode:
 *   - Reads from the inbound plain socket.
 *   - Writes to the outbound remote client, using TLS when enabled.
 * - In server mode:
 *   - Reads from the inbound TLS socket.
 *   - Writes to the outbound plain remote socket.
 * - Continues relaying recursively until an error occurs.
 * - Shuts down the connection on read or write failure.
 */
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

    if (!tlsSocket_ || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    tlsSocket_->async_read_some(
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

/**
 * @brief Relays traffic from the remote side back to the inbound client side.
 *
 * @details
 * - In agent mode:
 *   - Reads from the outbound remote client, using TLS when enabled.
 *   - Writes back to the inbound plain socket.
 * - In server mode:
 *   - Reads from the outbound plain remote socket.
 *   - Writes back to the inbound TLS socket.
 * - Continues relaying recursively until an error occurs.
 * - Shuts down the connection on read or write failure.
 */
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

    if (!tlsSocket_ || !client_->isOpen()) {
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
                                *self->tlsSocket_,
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

/**
 * @brief Starts or resets the timeout timer for this connection.
 *
 * @details
 * - Does nothing when the configured timeout value is zero.
 * - Schedules an asynchronous wait that invokes `onTimeout()`.
 */
void TCPConnection::resetTimeout() {
    if (!config_->general().timeout) return;

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));
    timeout_.async_wait(boost::bind(&TCPConnection::onTimeout, shared_from_this(),
                                    boost::asio::placeholders::error));
}

/**
 * @brief Cancels the active timeout timer.
 *
 * @details
 * - Does nothing when the configured timeout value is zero.
 */
void TCPConnection::cancelTimeout() {
    if (config_->general().timeout) timeout_.cancel();
}

/**
 * @brief Handles timeout expiration events.
 *
 * @details
 * - Ignores callbacks caused by cancellation or other non-expiration errors.
 * - Logs the timeout expiration.
 * - Shuts down the connection when the timeout expires.
 *
 * @param error Boost system error code associated with the timer event.
 */
void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;

    log_->write("[" + to_string(uuid_) +
                        "] [TCPConnection onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);
    socketShutdown();
}

/**
 * @brief Gracefully shuts down this connection and its associated outbound client.
 *
 * @details
 * - Shuts down and closes the inbound TLS socket when present.
 * - Shuts down and closes the inbound plain socket when open.
 * - Shuts down the associated outbound `TCPClient` when present.
 * - Ignores shutdown-related error codes.
 * - Logs any exception raised during shutdown.
 */
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