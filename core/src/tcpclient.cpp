#include "tcpclient.hpp"

#include <openssl/ssl.h>

/**
 * @brief Constructs a `TCPClient` instance with I/O context, configuration,
 *        and logging support.
 *
 * @details
 * - Stores references to the configuration, logging object, and I/O context.
 * - Initializes the plain TCP socket.
 * - Initializes the TLS context in client mode.
 * - Starts with TLS disabled and no SSL stream allocated.
 * - Initializes the DNS resolver and timeout timer.
 * - Sets the internal connection completion flag `end_` to `false`.
 *
 * @param io_context Reference to the Boost.Asio I/O context.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 */
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

/**
 * @brief Returns the underlying plain TCP socket.
 *
 * @return Reference to the internal TCP socket.
 */
TCPClient::tcp::socket &TCPClient::socket() { return socket_; }

/**
 * @brief Returns the TLS stream socket.
 *
 * @return Reference to the internal SSL stream.
 */
TCPClient::ssl_stream &TCPClient::sslSocket() { return *sslSocket_; }

/**
 * @brief Indicates whether TLS is enabled for this client.
 *
 * @return `true` if TLS is enabled, otherwise `false`.
 */
bool TCPClient::tlsEnabled() const { return tlsEnabled_; }

/**
 * @brief Checks whether the active underlying socket is open.
 *
 * @details
 * - If TLS is enabled and the SSL stream exists, checks the lowest layer socket.
 * - Otherwise checks the plain TCP socket.
 *
 * @return `true` if the active socket is open, otherwise `false`.
 */
bool TCPClient::isOpen() const {
    if (tlsEnabled_ && sslSocket_) return sslSocket_->lowest_layer().is_open();
    return socket_.is_open();
}

/**
 * @brief Enables TLS support for the client connection.
 *
 * @details
 * - Reinitializes the SSL context in TLS client mode.
 * - Applies SSL context options to disable legacy SSL versions.
 * - Enforces a minimum protocol version of TLS 1.2.
 * - Configures peer verification based on the application configuration.
 * - Loads the CA file when peer verification is enabled and a CA file is provided.
 * - Sets the allowed TLS cipher list and TLS 1.3 cipher suites.
 * - Creates the SSL stream object bound to the internal I/O context.
 * - Marks the client as TLS-enabled.
 *
 * @return `true` if TLS initialization succeeds, otherwise `false`.
 */
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

/**
 * @brief Resolves and connects to the specified destination.
 *
 * @details
 * - Resolves the destination host and port using the internal resolver.
 * - Connects either the plain TCP socket or the TLS socket's lowest layer,
 *   depending on whether TLS is enabled.
 * - Logs connection attempts and resolution/connect failures.
 *
 * @param dstIP Destination hostname or IP address.
 * @param dstPort Destination port.
 * @return `true` if the connection succeeds, otherwise `false`.
 */
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

/**
 * @brief Performs the TLS client handshake.
 *
 * @details
 * - Returns immediately if TLS is not enabled.
 * - Extracts the hostname from `config_->general().fakeUrl` for use as SNI.
 * - Strips the scheme, path, and port from the configured URL before setting SNI.
 * - Sets the TLS SNI extension on the SSL stream.
 * - Starts a timeout timer before the handshake and cancels it after completion.
 * - Logs SNI usage and handshake failures.
 *
 * @return `true` if the handshake succeeds or TLS is not enabled, otherwise `false`.
 */
bool TCPClient::doHandshakeClient() {
    std::lock_guard lock(mutex_);

    try {
        if (!tlsEnabled_ || !sslSocket_) return true;

        std::string sniHost = config_->general().fakeUrl;

        /**
         * @brief Extracts the hostname portion of the configured fake URL.
         */
        if (!sniHost.empty()) {
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

/**
 * @brief Replaces the internal write buffer with the contents of the provided buffer.
 *
 * @param buffer Source buffer whose contents are moved into the internal write buffer.
 */
void TCPClient::writeBuffer(boost::asio::streambuf &buffer) {
    std::lock_guard lock(mutex_);
    moveStreambuf(buffer, writeBuffer_);
}

/**
 * @brief Writes the contents of a buffer to the connected endpoint.
 *
 * @details
 * - Moves the provided input buffer into the internal write buffer.
 * - Verifies that the socket is open and that data exists to send.
 * - Starts a timeout timer before writing and cancels it afterwards.
 * - Writes through the TLS stream when TLS is enabled, otherwise uses the plain socket.
 * - Logs destination endpoint information and number of bytes written.
 * - Shuts down the socket on error.
 *
 * @param buffer Source buffer containing data to write.
 */
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

/**
 * @brief Reads agent-side data until the application terminator is encountered.
 *
 * @details
 * - Clears the internal read buffer before reading.
 * - Verifies that the socket is open.
 * - Starts a timeout timer before reading and cancels it afterwards.
 * - Reads until the delimiter `"COMP\r\n\r\n"` is found.
 * - Uses the TLS stream when TLS is enabled, otherwise uses the plain socket.
 * - Copies the received data into the general-purpose buffer on success.
 * - Shuts down the socket on EOF, read error, or exception.
 */
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

/**
 * @brief Reads a chunk of data from the remote server.
 *
 * @details
 * - Clears the internal read buffer before reading.
 * - Verifies that the socket is open.
 * - Starts a timeout timer before reading and cancels it afterwards.
 * - Reads up to 16384 bytes using `read_some`.
 * - Uses the TLS stream when TLS is enabled, otherwise uses the plain socket.
 * - Writes received bytes into the internal read buffer.
 * - Copies the received data into the general-purpose buffer.
 * - Updates the `end_` flag when no data is received.
 * - Logs source endpoint information and number of bytes read.
 * - Shuts down the socket on EOF, read error, or exception.
 */
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

/**
 * @brief Starts or resets the operation timeout timer.
 *
 * @details
 * - Does nothing when the configured timeout value is zero.
 * - Schedules an asynchronous wait that invokes `onTimeout()`.
 */
void TCPClient::resetTimeout() {
    if (!config_->general().timeout) return;

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));
    timeout_.async_wait(
            [self = shared_from_this()](const boost::system::error_code &error) {
                self->onTimeout(error);
            });
}

/**
 * @brief Cancels the active timeout timer.
 *
 * @details
 * - Does nothing when the configured timeout value is zero.
 */
void TCPClient::cancelTimeout() {
    if (config_->general().timeout) timeout_.cancel();
}

/**
 * @brief Handles timeout expiration events.
 *
 * @details
 * - Ignores callbacks triggered by cancellation or other non-expiration errors.
 * - Logs timeout expiration details.
 * - Shuts down the socket when the timeout expires.
 *
 * @param error Boost system error code for the timer event.
 */
void TCPClient::onTimeout(const boost::system::error_code &error) {
    if (error || error == boost::asio::error::operation_aborted) return;

    log_->write("[" + to_string(uuid_) +
                        "] [TCPClient onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);
    socketShutdown();
}

/**
 * @brief Shuts down and closes the active connection.
 *
 * @details
 * - Gracefully shuts down the TLS stream when present.
 * - Shuts down and closes the underlying socket.
 * - Ignores shutdown-related error codes.
 * - Logs exceptions encountered during shutdown.
 */
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