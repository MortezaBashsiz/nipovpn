#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"
#include "tcpconnection.hpp"

/**
 * @brief This class is to create and handle a TCP server.
 * It listens to the IP:Port and handles the socket.
 */
class TCPServer {
public:
    using pointer = boost::shared_ptr<TCPServer>;

    /**
   * @brief Factory method to create a TCPServer instance.
   *
   * @param io_context The io_context to be used for asynchronous operations.
   * @param config The shared configuration object.
   * @param log The shared log object.
   * @return pointer A shared pointer to the created TCPServer instance.
   */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPServer(io_context, config, log));
    }

private:
    /**
   * @brief Constructs a TCPServer instance.
   *
   * @param io_context The io_context to be used for asynchronous operations.
   * @param config The shared configuration object.
   * @param log The shared log object.
   */
    explicit TCPServer(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
   * @brief Starts accepting incoming connections.
   */
    void startAccept();

    /**
   * @brief Handles the acceptance of a new connection.
   *
   * @param connection The pointer to the new TCP connection.
   * @param error The error code from the accept operation.
   */
    void handleAccept(TCPConnection::pointer connection,
                      const boost::system::error_code &error);

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    TCPClient::pointer client_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;

    // Strand to ensure that handler functions are not called concurrently.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};
