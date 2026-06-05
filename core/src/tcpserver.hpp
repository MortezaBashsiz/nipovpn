#pragma once

#include <boost/asio.hpp>
#include <memory>

#include "tcpconnection.hpp"

class TCPServer {
public:
    using pointer = std::shared_ptr<TCPServer>;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPServer(io_context, config, log));
    }

    void startAccept();

private:
    explicit TCPServer(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    void handleAccept(TCPConnection::pointer connection,
                      const boost::system::error_code &error);

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};