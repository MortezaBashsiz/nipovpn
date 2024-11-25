#include "tcpserver.hpp"

/**
 * @brief Constructs a new TCPServer instance.
 * 
 * Initializes the `acceptor_` with the specified IP and port from the configuration, and sets the `reuse_address` option
 * to allow multiple bindings to the same address. This constructor also starts the process of accepting connections.
 * 
 * @param io_context The IO context for asynchronous operations.
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
                boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(config->listenIp()),
                                               config->listenPort())) {
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));

    startAccept();
}

/**
 * @brief Starts accepting incoming client connections asynchronously.
 * 
 * This method creates a new `TCPClient` and `TCPConnection` object to handle the connection, then starts the asynchronous
 * accept operation using the acceptor. Once a connection is accepted, the `handleAccept` method will be called.
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
 * @brief Handles the result of the asynchronous accept operation.
 * 
 * If the connection was successfully accepted, this method starts the communication on the connection by invoking the
 * `start` method. If an error or exception occurs, it logs the error or exception.
 * 
 * @param connection The `TCPConnection` object representing the accepted connection.
 * @param error The error code resulting from the accept operation.
 */
void TCPServer::handleAccept(TCPConnection::pointer connection,
                             const boost::system::error_code &error) {
    try {
        if (!error) {
            connection->start();
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

    startAccept();
}
