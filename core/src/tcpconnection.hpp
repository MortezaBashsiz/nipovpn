#pragma once

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <mutex>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

/**
 * @brief The `TCPConnection` class manages a single inbound connection and its forwarding lifecycle.
 *
 * @details
 * - Owns the accepted client-side socket.
 * - Optionally supports TLS on the server-facing accepted connection.
 * - Coordinates request handling using either `AgentHandler` or `ServerHandler`
 *   depending on the runtime mode.
 * - Manages asynchronous reads, writes, timeout handling, and tunnel relaying.
 * - Maintains per-connection state such as connection completion, CONNECT mode,
 *   and full tunnel mode.
 *
 * @note
 * - Inherits from `boost::enable_shared_from_this` to safely retain itself
 *   during asynchronous operations.
 * - Uses shared ownership via `boost::shared_ptr`.
 * - Instances should be created using the `create` factory method.
 */
class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer = boost::shared_ptr<TCPConnection>;
    using ssl_stream =
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    /**
     * @brief Factory method to create a new `TCPConnection` instance.
     *
     * @details
     * - Wraps construction in a `boost::shared_ptr`.
     * - Ensures consistent ownership and lifetime management for asynchronous use.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the associated outbound `TCPClient`.
     * @return Shared pointer to the created `TCPConnection`.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client) {
        return pointer(new TCPConnection(io_context, config, log, client));
    }

    /**
     * @brief Returns the accepted client-side socket.
     *
     * @return Reference to the underlying TCP socket.
     */
    boost::asio::ip::tcp::socket &socket();

    /**
     * @brief Returns the associated outbound `TCPClient`.
     *
     * @return Shared pointer to the client object.
     */
    inline TCPClient::pointer client() { return client_; }

    /**
     * @brief Returns the TLS server stream.
     *
     * @return Reference to the TLS socket stream.
     */
    ssl_stream &tlsSocket();

    /**
     * @brief Initializes the TLS server context for this connection.
     *
     * @details
     * - Loads certificates, private key, and related TLS configuration.
     * - Prepares the connection to accept inbound TLS handshakes.
     *
     * @return `true` if initialization succeeds, otherwise `false`.
     */
    bool initTlsServerContext();

    /**
     * @brief Performs the server-side TLS handshake.
     *
     * @return `true` if the handshake succeeds, otherwise `false`.
     */
    bool doHandshakeServer();

    /**
     * @brief Replaces the internal write buffer with the provided buffer contents.
     *
     * @param buffer Source buffer whose contents are moved into the internal write buffer.
     */
    inline void writeBuffer(boost::asio::streambuf &buffer) {
        moveStreambuf(buffer, writeBuffer_);
    }

    /**
     * @brief Returns the internal read buffer (lvalue).
     *
     * @return Const reference to the internal read buffer.
     */
    inline const boost::asio::streambuf &readBuffer() & { return readBuffer_; }

    /**
     * @brief Returns the internal read buffer (rvalue).
     *
     * @return Const rvalue reference to the internal read buffer.
     */
    inline const boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    /**
     * @brief Returns the UUID associated with this connection.
     *
     * @return Connection UUID.
     */
    inline boost::uuids::uuid uuid() { return uuid_; }

    /**
     * @brief Starts connection processing in agent mode.
     *
     * @details
     * - Uses `AgentHandler` logic to process incoming traffic.
     */
    void startAgent();

    /**
     * @brief Starts connection processing in server mode.
     *
     * @details
     * - Uses `ServerHandler` logic to process incoming traffic.
     */
    void startServer();

    /**
     * @brief Initiates an asynchronous read for agent-side traffic.
     */
    void doReadAgent();

    /**
     * @brief Initiates an asynchronous read for server-side traffic.
     */
    void doReadServer();

    /**
     * @brief Handles completion of an agent-side asynchronous read.
     *
     * @param error Boost system error code.
     * @param bytes_transferred Number of bytes received.
     */
    void handleReadAgent(const boost::system::error_code &error,
                         size_t bytes_transferred);

    /**
     * @brief Handles completion of a server-side asynchronous read.
     *
     * @param error Boost system error code.
     * @param bytes_transferred Number of bytes received.
     */
    void handleReadServer(const boost::system::error_code &error,
                          size_t bytes_transferred);

    /**
     * @brief Gracefully shuts down and closes the active connection.
     */
    void socketShutdown();

    /**
     * @brief Enables full tunnel relay mode.
     *
     * @details
     * - Used after CONNECT-style negotiation when traffic should be relayed
     *   transparently in both directions.
     */
    void enableTunnelMode();

    /**
     * @brief Relays traffic from the connected client to the remote endpoint.
     */
    void relayClientToRemote();

    /**
     * @brief Relays traffic from the remote endpoint back to the connected client.
     */
    void relayRemoteToClient();

private:
    /**
     * @brief Constructs a `TCPConnection` instance.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the associated outbound `TCPClient`.
     */
    explicit TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           TCPClient::pointer client);

    /**
     * @brief Starts or resets the timeout timer for the connection.
     */
    void resetTimeout();

    /**
     * @brief Cancels the active timeout timer.
     */
    void cancelTimeout();

    /**
     * @brief Handles timeout expiration events.
     *
     * @param error Boost system error code for the timer event.
     */
    void onTimeout(const boost::system::error_code &error);

    boost::asio::ip::tcp::socket socket_;  ///< Accepted inbound socket.
    boost::asio::ssl::context sslContext_; ///< TLS server context.
    std::unique_ptr<ssl_stream> tlsSocket_;///< Optional TLS-wrapped inbound socket.

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.
    boost::asio::io_context &io_context_;  ///< Reference to the I/O context.
    TCPClient::pointer client_;            ///< Associated outbound TCP client.

    boost::asio::streambuf readBuffer_; ///< Buffer for incoming data.
    boost::asio::streambuf writeBuffer_;///< Buffer for outgoing data.

    boost::asio::steady_timer timeout_;                                 ///< Per-connection timeout timer.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;///< Strand for serialized handlers.

    boost::uuids::uuid uuid_;///< Unique identifier for this connection.
    std::mutex mutex_;       ///< Mutex for thread-safe state changes.

    bool end_;       ///< Indicates whether processing has completed.
    bool connect_;   ///< Indicates whether a connection or CONNECT tunnel is active.
    bool tunnelMode_;///< Indicates whether transparent bidirectional relay mode is enabled.

    std::array<char, 16384> downstreamBuf_;///< Buffer for traffic flowing to the remote side.
    std::array<char, 16384> upstreamBuf_;  ///< Buffer for traffic flowing back to the client.
};