#pragma once

#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/**
 * @brief The `ServerHandler` class manages server-side request processing for a single connection.
 *
 * @details
 * - Responsible for handling incoming client data and generating responses.
 * - Integrates with the `HTTP` parser for request processing.
 * - Uses `TCPClient` to manage the underlying connection.
 * - Maintains connection state such as completion and CONNECT tunneling.
 * - Ensures thread-safe operations using an internal mutex.
 *
 * @note
 * - Inherits from `Uncopyable` to prevent copying.
 * - Uses shared ownership via `std::shared_ptr`.
 * - Instances should be created using the `create` factory method.
 */
class ServerHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<ServerHandler>;

    /**
     * @brief Factory method to create a new `ServerHandler` instance.
     *
     * @details
     * - Wraps construction in a `std::shared_ptr`.
     * - Ensures consistent ownership and lifecycle management.
     *
     * @param readBuffer Reference to the stream buffer for incoming data.
     * @param writeBuffer Reference to the stream buffer for outgoing data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the associated `TCPClient`.
     * @param clientConnStr String describing the client connection (e.g., IP:port).
     * @param uuid Unique identifier for this connection.
     * @return A shared pointer to the created `ServerHandler`.
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
     * @brief Destructor for `ServerHandler`.
     *
     * @details
     * - Performs no explicit cleanup.
     * - Resource management is handled by referenced objects and smart pointers.
     */
    ~ServerHandler();

    /**
     * @brief Processes the incoming request and generates the appropriate response.
     *
     * @details
     * - Parses incoming data using the `HTTP` handler.
     * - Handles connection logic, including CONNECT tunneling.
     * - Coordinates communication with the client and remote endpoints.
     */
    void handle();

    /**
     * @brief Returns a reference to the associated HTTP request object.
     *
     * @return Const reference to the HTTP request pointer.
     */
    inline const HTTP::pointer &request() & { return request_; }

    /**
     * @brief Returns an rvalue reference to the HTTP request object.
     *
     * @details
     * - Enables move semantics when transferring ownership.
     *
     * @return Rvalue reference to the HTTP request pointer.
     */
    inline const HTTP::pointer &&request() && { return std::move(request_); }

    bool end_;    ///< Indicates whether request processing is complete.
    bool connect_;///< Indicates whether a CONNECT tunnel or persistent connection is active.

private:
    /**
     * @brief Constructs a `ServerHandler` instance.
     *
     * @details
     * - Initializes buffers, configuration, logging, and client references.
     * - Creates the internal HTTP request handler.
     * - Stores connection metadata and unique identifier.
     *
     * @param readBuffer Reference to the input stream buffer.
     * @param writeBuffer Reference to the output stream buffer.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the TCP client.
     * @param clientConnStr Client connection description string.
     * @param uuid Unique identifier for this handler instance.
     */
    explicit ServerHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client,
                           const std::string &clientConnStr,
                           boost::uuids::uuid uuid);

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.
    const TCPClient::pointer &client_;     ///< Reference to the TCP client.
    boost::asio::streambuf &readBuffer_;   ///< Buffer for incoming data.
    boost::asio::streambuf &writeBuffer_;  ///< Buffer for outgoing data.
    HTTP::pointer request_;                ///< HTTP request handler instance.
    const std::string &clientConnStr_;     ///< Client connection string.
    boost::uuids::uuid uuid_;              ///< Unique identifier for this handler.

    std::mutex mutex_;///< Mutex for thread-safe operations.
};