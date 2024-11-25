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
 * @brief A TCP server class that accepts incoming TCP connections and handles them.
 * 
 * The class listens for incoming connections and creates `TCPConnection` objects to handle communication.
 */
class TCPServer {
public:
    using pointer = boost::shared_ptr<TCPServer>;

    /**
     * @brief Creates a new TCPServer instance.
     * 
     * This static factory method initializes a `TCPServer` with the provided IO context, configuration, and logging.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * 
     * @return A shared pointer to the newly created TCPServer instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPServer(io_context, config, log));
    }
    /**
     * @brief Starts the process of accepting incoming connections.
     * 
     * This method begins the asynchronous accept operation to listen for incoming client connections.
     */
    void startAccept();

private:
    /**
     * @brief Constructs a TCPServer object.
     * 
     * Initializes the acceptor, sets up the IO context, and stores the configuration and log objects.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPServer(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
     * @brief Handles the completion of an accept operation.
     * 
     * Once a new connection is accepted, this method is called to handle the connection by creating a `TCPConnection`
     * object and starting communication with the client.
     * 
     * @param connection A shared pointer to the new `TCPConnection` object.
     * @param error The error code indicating the result of the accept operation.
     */
    void handleAccept(TCPConnection::pointer connection,
                      const boost::system::error_code &error);

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};
