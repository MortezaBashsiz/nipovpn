#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"
#include "tcpconnection.hpp"

class TCPServer {
public:
    using pointer = boost::shared_ptr<TCPServer>;

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

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    TCPClient::pointer client_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};
