#include "tcpserver.hpp"

#include <atomic>
#include <chrono>

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

void TCPServer::startAccept() {
    auto client = TCPClient::create(io_context_, config_, log_);
    auto connection = TCPConnection::create(io_context_, config_, log_, client);

    acceptor_.async_accept(
            connection->socket(),
            [this, connection](const boost::system::error_code &error) {
                handleAccept(connection, error);
            });
}

void TCPServer::handleAccept(
        TCPConnection::pointer connection,
        const boost::system::error_code &error) {
    startAccept();

    try {
        if (error) {
            log_->write("[TCPServer handleAccept] Error: " + error.message(), Log::Level::ERROR);
            return;
        }

        if (config_->runMode() == RunMode::server) {
            if (config_->general().tlsEnable) {
                if (!connection->initTlsServerContext()) {
                    log_->write("[TCPServer handleAccept] TLS server context init failed", Log::Level::ERROR);
                    connection->socketShutdown();
                    return;
                }

                connection->tlsSocket().lowest_layer() = std::move(connection->socket());

                if (!connection->doHandshakeServer()) {
                    log_->write("[TCPServer handleAccept] TLS server handshake failed", Log::Level::ERROR);
                    connection->socketShutdown();
                    return;
                }
            }

            connection->startServer();
        } else {
            connection->startAgent();
        }
    } catch (const std::exception &ex) {
        log_->write("[TCPServer handleAccept] Exception: " + std::string(ex.what()), Log::Level::ERROR);
        connection->socketShutdown();
    } catch (...) {
        log_->write("[TCPServer handleAccept] Unknown exception", Log::Level::ERROR);
        connection->socketShutdown();
    }
}