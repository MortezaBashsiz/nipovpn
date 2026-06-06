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

void TCPServer::handleAccept(TCPConnection::pointer connection,
                             const boost::system::error_code &error) {
    /*
     * IMPORTANT:
     * Do not wait for TLS handshake before accepting the next client.
     * Bad/half-open TLS clients can otherwise occupy all io threads and
     * the server looks frozen until restart.
     */
    startAccept();

    try {
        if (error) {
            log_->write("[TCPServer handleAccept] Error: " + error.message(),
                        Log::Level::ERROR);
            return;
        }

        if (config_->runMode() != RunMode::server) {
            connection->startAgent();
            return;
        }

        if (!config_->general().tlsEnable) {
            connection->startServer();
            return;
        }

        if (!connection->initTlsServerContext()) {
            log_->write("[TCPServer handleAccept] TLS server context init failed",
                        Log::Level::ERROR);
            connection->socketShutdown();
            return;
        }

        connection->tlsSocket().lowest_layer() = std::move(connection->socket());

        auto done = std::make_shared<std::atomic_bool>(false);
        auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);

        timer->expires_after(std::chrono::seconds(5));
        timer->async_wait([this, connection, done](const boost::system::error_code &ec) {
            if (ec) {
                return;
            }

            if (!done->exchange(true)) {
                log_->write("[TCPServer handleAccept] TLS server handshake timeout",
                            Log::Level::DEBUG);
                connection->socketShutdown();
            }
        });

        connection->tlsSocket().async_handshake(
                boost::asio::ssl::stream_base::server,
                [this, connection, timer, done](const boost::system::error_code &ec) {
                    timer->cancel();

                    if (done->exchange(true)) {
                        return;
                    }

                    if (ec) {
                        /*
                         * stream truncated is common when random scanners or
                         * non-Nipo clients hit port 443. Do not treat it as fatal.
                         */
                        log_->write("[TCPServer handleAccept] TLS server handshake failed: " + ec.message(),
                                    Log::Level::DEBUG);
                        connection->socketShutdown();
                        return;
                    }

                    connection->startServer();
                });

    } catch (const std::exception &ex) {
        log_->write("[TCPServer handleAccept] Exception: " + std::string(ex.what()),
                    Log::Level::ERROR);
        connection->socketShutdown();
    } catch (...) {
        log_->write("[TCPServer handleAccept] Unknown exception",
                    Log::Level::ERROR);
        connection->socketShutdown();
    }
}