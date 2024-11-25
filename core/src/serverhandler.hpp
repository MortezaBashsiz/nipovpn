#pragma once

#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/**
 * @class ServerHandler
 * @brief Handles server-side processing for a specific connection.
 *
 * The `ServerHandler` manages incoming requests, outgoing responses, and the connection state.
 * It is tightly coupled with the configuration, logging, and client connection objects.
 */
class ServerHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<ServerHandler>;

    /**
     * @brief Factory method to create a new `ServerHandler` instance.
     *
     * @param readBuffer Reference to the read buffer for incoming data.
     * @param writeBuffer Reference to the write buffer for outgoing data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging instance.
     * @param client Pointer to the TCP client associated with the connection.
     * @param clientConnStr String representing the client connection (e.g., IP and port).
     * @param uuid UUID associated with the connection.
     * @return Shared pointer to the newly created `ServerHandler` instance.
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

    /**
     * @brief Destructor for the `ServerHandler` class.
     */
    ~ServerHandler();

    /**
     * @brief Handles incoming requests and manages responses.
     *
     * This function is responsible for processing the request and initiating
     * the necessary actions based on the server's business logic.
     */
    void handle();

    /**
     * @brief Continues reading from the connection.
     *
     * This function is invoked when additional data is expected to be read.
     */
    void continueRead();

    /**
     * @brief Returns a reference to the HTTP request object.
     *
     * Provides access to the request being handled by the server.
     * 
     * @return Reference to the HTTP request object.
     */
    inline const HTTP::pointer &request() & { return request_; }

    /**
     * @brief Returns an rvalue reference to the HTTP request object.
     *
     * Allows moving the HTTP request object when necessary.
     * 
     * @return Rvalue reference to the HTTP request object.
     */
    inline const HTTP::pointer &&request() && { return std::move(request_); }

    bool end_;
    bool connect_;

private:
    /**
     * @brief Constructor for `ServerHandler`.
     *
     * Initializes the handler with the given buffers, configuration, log, client,
     * connection string, and UUID.
     *
     * @param readBuffer Reference to the read buffer for incoming data.
     * @param writeBuffer Reference to the write buffer for outgoing data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging instance.
     * @param client Pointer to the TCP client associated with the connection.
     * @param clientConnStr String representing the client connection (e.g., IP and port).
     * @param uuid UUID associated with the connection.
     */
    explicit ServerHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client,
                           const std::string &clientConnStr,
                           boost::uuids::uuid uuid);
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    const TCPClient::pointer &client_;
    boost::asio::streambuf &readBuffer_;
    boost::asio::streambuf &writeBuffer_;
    HTTP::pointer request_;
    const std::string &clientConnStr_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;
};
