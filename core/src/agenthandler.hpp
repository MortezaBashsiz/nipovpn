#pragma once

#include <boost/beast/core/detail/base64.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/**
 * @brief The `AgentHandler` class manages communication between the client and the agent server.
 *
 * @details
 * - Responsible for processing incoming client requests and forwarding them to the agent server.
 * - Handles encryption, HTTP request generation, and response processing.
 * - Maintains connection state and supports both normal HTTP and CONNECT tunneling.
 * - Uses Boost.Asio stream buffers for efficient I/O operations.
 * - Ensures thread safety using an internal mutex.
 * - Integrates with external components such as configuration, logging, and TCP client handling.
 *
 * @note
 * - Inherits from `Uncopyable` to prevent accidental copying.
 * - Uses shared ownership via `std::shared_ptr` (see `pointer` alias).
 * - Instances must be created using the `create` factory method.
 */
class AgentHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<AgentHandler>;

    /**
     * @brief Factory method to create a new `AgentHandler` instance.
     *
     * @details
     * - Wraps object construction in a `std::shared_ptr`.
     * - Ensures consistent creation and ownership semantics.
     *
     * @param readBuffer Reference to the stream buffer used for reading client data.
     * @param writeBuffer Reference to the stream buffer used for writing response data.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the `TCPClient` managing server communication.
     * @param clientConnStr String describing the client connection source.
     * @param uuid Unique identifier for this handler instance.
     * @return A shared pointer to the created `AgentHandler`.
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

    /**
     * @brief Destructor for `AgentHandler`.
     *
     * @details
     * - Default destructor; resource management is handled by smart pointers and references.
     */
    ~AgentHandler();

    /**
     * @brief Processes a client request and communicates with the agent server.
     *
     * @details
     * - Encrypts incoming data.
     * - Builds and sends an HTTP request to the agent server.
     * - Receives, parses, and decrypts the server response.
     * - Handles CONNECT tunneling and normal HTTP flows.
     */
    void handle();

    /**
     * @brief Provides access to the associated HTTP request object.
     *
     * @return A const reference to the HTTP request pointer.
     */
    inline const HTTP::pointer &request() & { return request_; }

    /**
     * @brief Provides rvalue access to the HTTP request object.
     *
     * @details
     * - Enables move semantics when transferring ownership.
     *
     * @return An rvalue reference to the HTTP request pointer.
     */
    inline const HTTP::pointer &&request() && { return std::move(request_); }

    bool end_;    ///< Indicates whether processing has completed.
    bool connect_;///< Indicates whether a CONNECT tunnel or server connection is active.

private:
    /**
     * @brief Constructs an `AgentHandler` instance.
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
    AgentHandler(boost::asio::streambuf &readBuffer,
                 boost::asio::streambuf &writeBuffer,
                 const std::shared_ptr<Config> &config,
                 const std::shared_ptr<Log> &log,
                 const TCPClient::pointer &client,
                 const std::string &clientConnStr,
                 boost::uuids::uuid uuid);

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.
    const TCPClient::pointer &client_;     ///< Reference to the TCP client.
    boost::asio::streambuf &readBuffer_;   ///< Buffer for incoming client data.
    boost::asio::streambuf &writeBuffer_;  ///< Buffer for outgoing response data.
    HTTP::pointer request_;                ///< HTTP request handler instance.
    const std::string &clientConnStr_;     ///< Client connection string.

    boost::uuids::uuid uuid_;///< Unique identifier for this handler.

    std::mutex mutex_;///< Mutex used to ensure thread-safe operations.
};