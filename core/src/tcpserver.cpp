#include "tcpserver.hpp"

/**
 * @brief Constructs a `TCPServer` instance and initializes the listening acceptor.
 *
 * @details
 * - Binds the acceptor to the configured IP address and port.
 * - Enables `reuse_address` to allow quick restarts without waiting for socket release.
 * - Immediately starts accepting incoming connections.
 *
 * @param io_context Reference to the Boost.Asio I/O context.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 */
TCPServer::TCPServer(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      io_context_(io_context),
      acceptor_(io_context,
                boost::asio::ip::tcp::endpoint(
                        boost::asio::ip::make_address(config->listenIp()),
                        config->listenPort())) {
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));

    startAccept();
}

/**
 * @brief Initiates an asynchronous accept operation for incoming connections.
 *
 * @details
 * - Creates a new `TCPClient` and associated `TCPConnection`.
 * - Calls `async_accept` to wait for incoming connections.
 * - On completion, forwards control to `handleAccept()`.
 */
void TCPServer::startAccept() {
    auto client = TCPClient::create(io_context_, config_, log_);
    auto connection = TCPConnection::create(io_context_, config_, log_, client);

    acceptor_.async_accept(
            connection->socket(),
            [this, connection](const boost::system::error_code &error) {
                handleAccept(connection, error);
            });
}

/**
 * @brief Handles the result of an asynchronous accept operation.
 *
 * @details
 * - On successful accept:
 *   - In **server mode**:
 *     - Initializes TLS server context.
 *     - Transfers the accepted socket to the TLS stream.
 *     - Performs TLS handshake.
 *     - Starts server-side processing.
 *   - In **agent mode**:
 *     - Starts agent-side processing directly (no TLS termination).
 *
 * - On failure:
 *   - Logs the error.
 *
 * - Always continues accepting new connections regardless of outcome.
 *
 * @param connection Shared pointer to the accepted `TCPConnection`.
 * @param error Error code returned by the accept operation.
 */
void TCPServer::handleAccept(TCPConnection::pointer connection,
                             const boost::system::error_code &error) {
    try {
        if (!error) {
            if (config_->runMode() == RunMode::server) {
                // Initialize TLS context for incoming server connections
                if (!connection->initTlsServerContext()) {
                    log_->write("[TCPServer handleAccept] TLS server context init failed",
                                Log::Level::ERROR);
                    connection->socketShutdown();
                    startAccept();
                    return;
                }

                // Move plain socket into TLS layer
                connection->tlsSocket().lowest_layer() = std::move(connection->socket());

                // Perform TLS handshake
                if (!connection->doHandshakeServer()) {
                    log_->write("[TCPServer handleAccept] TLS server handshake failed",
                                Log::Level::ERROR);
                    connection->socketShutdown();
                    startAccept();
                    return;
                }

                // Start server-side handling
                connection->startServer();
            } else {
                // Start agent-side handling (no TLS termination)
                connection->startAgent();
            }
        } else {
            log_->write("[TCPServer handleAccept] Error: " + error.message(),
                        Log::Level::ERROR);
        }
    } catch (const std::exception &ex) {
        log_->write("[TCPServer handleAccept] Exception: " + std::string(ex.what()),
                    Log::Level::ERROR);
    } catch (...) {
        log_->write("[TCPServer handleAccept] Unknown exception",
                    Log::Level::ERROR);
    }

    // Continue accepting new connections
    startAccept();
}