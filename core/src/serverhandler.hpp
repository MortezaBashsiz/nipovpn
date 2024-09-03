#pragma once
#ifndef SERVERHANDLER_HPP
#define SERVERHANDLER_HPP

#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/*
 * The ServerHandler class is responsible for managing and handling HTTP/HTTPS
 * requests when the process is running in server mode. It is initialized with
 * buffers for reading and writing data, as well as configuration and logging
 * objects, and a TCP client object for communication.
 */
class ServerHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<ServerHandler>;// Type alias for shared
                                                   // pointer to ServerHandler.

    /*
   * Static factory method to create a shared pointer to a ServerHandler
   * instance.
   * @param readBuffer - Buffer used for reading incoming data.
   * @param writeBuffer - Buffer used for writing outgoing data.
   * @param config - Shared pointer to the configuration object.
   * @param log - Shared pointer to the logging object.
   * @param client - Shared pointer to the TCP client object.
   * @return A shared pointer to the created ServerHandler instance.
   */
    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client,
                          const std::string &clientConnStr,
                          boost::uuids::uuid uuid) {
        return pointer(new ServerHandler(readBuffer, writeBuffer, config, log,
                                         client, clientConnStr, uuid));
    }

    /*
   * Destructor to clean up resources.
   */
    ~ServerHandler();

    /*
   * Handles incoming requests by processing them based on their type
   * (HTTP/HTTPS). This method is invoked from the TCPConnection::handleRead
   * function.
   */
    void handle();

    /*
   * Accessor for the HTTP request object.
   * @return A reference to the HTTP request object.
   */
    inline const HTTP::pointer &request() & { return request_; }

    /*
   * Move accessor for the HTTP request object.
   * @return An rvalue reference to the HTTP request object.
   */
    inline const HTTP::pointer &&request() && { return std::move(request_); }

private:
    /*
   * Constructor to initialize the ServerHandler object.
   * @param readBuffer - Buffer used for reading incoming data.
   * @param writeBuffer - Buffer used for writing outgoing data.
   * @param config - Shared pointer to the configuration object.
   * @param log - Shared pointer to the logging object.
   * @param client - Shared pointer to the TCP client object.
   */
    explicit ServerHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client,
                           const std::string &clientConnStr,
                           boost::uuids::uuid uuid);

    const std::shared_ptr<Config>
            &config_;                 // Reference to the configuration object.
    const std::shared_ptr<Log> &log_; // Reference to the logging object.
    const TCPClient::pointer &client_;// Reference to the TCP client object.
    boost::asio::streambuf &readBuffer_,
            &writeBuffer_; // References to the read and write buffers.
    HTTP::pointer request_;// Shared pointer to the HTTP request object.
    const std::string
            &clientConnStr_;///< socket client connection string "ip:port"

    boost::uuids::uuid uuid_;

    std::mutex mutex_;///< Mutex to make the class thread-safe
};

#endif /* SERVERHANDLER_HPP */
