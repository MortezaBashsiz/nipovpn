#pragma once

#include <boost/beast/core/detail/base64.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/**
 * @brief The `AgentHandler` class manages the interaction between the client and the agent, 
 *        including handling requests, responses, and connection state.
 *
 * @details
 * - This class is derived from `Uncopyable` to prevent copying or assignment.
 * - It provides functionality for handling initial requests (`handle`) and continuing 
 *   with subsequent reads and responses (`continueRead`).
 * - Uses Boost.Asio stream buffers for managing input and output data streams.
 * - Ensures thread safety with an internal mutex.
 * - Maintains the connection state with `end_` and `connect_` flags.
 * - Supports logging and configuration through shared pointers to external objects.
 *
 * @note
 * - The class uses a `pointer` alias for shared ownership management via `std::shared_ptr`.
 * - The `create` static factory method constructs an instance and returns it as a `pointer`.
 * - Requires a valid Boost.Asio I/O context and other dependencies to function properly.
 */
class AgentHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<AgentHandler>;

    /**
     * @brief Factory method to create an instance of `AgentHandler`.
     *
     * @param readBuffer Reference to the stream buffer for reading data.
     * @param writeBuffer Reference to the stream buffer for writing data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the `TCPClient` object managing the connection.
     * @param clientConnStr String containing the client connection information.
     * @param uuid Unique identifier for this handler, represented as a Boost UUID.
     * @return A shared pointer to the newly created `AgentHandler` instance.
     */
    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client,
                          const std::string &clientConnStr,
                          boost::uuids::uuid uuid) {
        return pointer(new AgentHandler(readBuffer, writeBuffer, config, log,
                                        client, clientConnStr, uuid));
    }

    ~AgentHandler();

    /**
     * @brief Handles the initial processing of requests, encryption, and server communication.
     */
    void handle();

    /**
     * @brief Continues processing subsequent reads, handling responses, and decryption.
     */
    void continueRead();

    /**
     * @brief Accessor for the HTTP request object.
     *
     * @return A reference to the HTTP request object.
     */
    inline const HTTP::pointer &request() & { return request_; }

    /**
     * @brief Accessor for the HTTP request object, allowing move semantics.
     *
     * @return An rvalue reference to the HTTP request object.
     */
    inline const HTTP::pointer &&request() && { return std::move(request_); }

    bool end_;    ///< Indicates whether the handler has completed its operations.
    bool connect_;///< Indicates the connection state.

private:
    /**
     * @brief Constructs an `AgentHandler` instance with the specified parameters.
     *
     * @param readBuffer Reference to the stream buffer for reading data.
     * @param writeBuffer Reference to the stream buffer for writing data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the `TCPClient` object managing the connection.
     * @param clientConnStr String containing the client connection information.
     * @param uuid Unique identifier for this handler, represented as a Boost UUID.
     */
    AgentHandler(boost::asio::streambuf &readBuffer,
                 boost::asio::streambuf &writeBuffer,
                 const std::shared_ptr<Config> &config,
                 const std::shared_ptr<Log> &log,
                 const TCPClient::pointer &client,
                 const std::string &clientConnStr,
                 boost::uuids::uuid uuid);

    const std::shared_ptr<Config> &config_;///< Configuration object reference.
    const std::shared_ptr<Log> &log_;      ///< Logging object reference.
    const TCPClient::pointer &client_;     ///< TCP client managing the connection.
    boost::asio::streambuf &readBuffer_;   ///< Buffer for reading data.
    boost::asio::streambuf &writeBuffer_;  ///< Buffer for writing data.
    HTTP::pointer request_;                ///< HTTP request handler.
    const std::string &clientConnStr_;     ///< Client connection string.

    boost::uuids::uuid uuid_;///< Unique identifier for this handler.

    std::mutex mutex_;///< Mutex for ensuring thread safety.
};
