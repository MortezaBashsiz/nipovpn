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

    /**
     * @brief Factory method to create a new TCPServer instance.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return Shared pointer to a new TCPServer instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log);

    /**
     * @brief Starts accepting incoming connections.
     */
    void startAccept();

private:
    /**
     * @brief Initializes the TCP server.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPServer(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
     * @brief Handles the acceptance of a new connection.
     * @param connection The pointer to the new TCP connection.
     * @param error The error code from the accept operation.
     */
    void handleAccept(TCPConnection::pointer connection,
                      const boost::system::error_code &error);

private:
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    TCPClient::pointer client_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};
