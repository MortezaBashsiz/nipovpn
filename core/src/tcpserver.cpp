#include "tcpserver.hpp"


TCPServer::TCPServer(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),
      log_(log),
      client_(TCPClient::create(io_context, config, log)),
      io_context_(io_context),
      acceptor_(io_context,
                boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(config->listenIp()),
                                               config->listenPort())) {
    startAccept();
}

void TCPServer::startAccept() {
    auto connection = TCPConnection::create(io_context_, config_, log_, client_);

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.async_accept(
            connection->getSocket(),
            [this, connection](const boost::system::error_code &error) {
                handleAccept(connection, error);
            });
}

void TCPServer::handleAccept(TCPConnection::pointer connection,
                             const boost::system::error_code &error) {
    if (!error) {
        connection->start();
    } else {
        log_->write("[TCPServer handleAccept] " + std::string(error.message()),
                    Log::Level::ERROR);
    }
    startAccept();
}
